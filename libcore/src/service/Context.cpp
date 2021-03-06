/*  Sirikata
 *  Context.cpp
 *
 *  Copyright (c) 2010, Ewen Cheslack-Postava
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Sirikata nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sirikata/core/util/Standard.hh>
#include <sirikata/core/service/Context.hpp>
#include <sirikata/core/network/IOStrandImpl.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <sirikata/core/service/Breakpad.hpp>
#include <sirikata/core/command/Commander.hpp>

#define CTX_LOG(lvl, msg) SILOG(context, lvl, msg)

namespace Sirikata {

Context::Context(const String& name_, Network::IOService* ios, Network::IOStrand* strand, Trace::Trace* _trace, const Time& epoch, const Duration& simlen)
 : name(name_),
   ioService(ios),
   mainStrand(strand),
   profiler(NULL),
   timeSeries(NULL),
   mFinishedTimer( Network::IOTimer::create(ios) ),
   mTrace(_trace),
   mCommander(NULL),
   mEpoch(epoch),
   mLastSimTime(Time::null()),
   mSimDuration(simlen),
   mKillThread(),
   mKillService(NULL),
   mKillTimer(),
   mStopRequested(false)
{
    CTX_LOG(info, "Creating context");
  Breakpad::init();
  profiler = new TimeProfiler(this, name);
}

Context::~Context() {
    CTX_LOG(info, "Destroying context");
    delete profiler;
}

void Context::start() {
    mSignalHandler = Signal::registerHandler(
        std::tr1::bind(&Context::handleSignal, this, std::tr1::placeholders::_1)
    );

    if (mSimDuration == Duration::zero())
        return;

    Time t_now = simTime();
    Time t_end = simTime(mSimDuration);
    Duration wait_dur = t_end - t_now;
    mFinishedTimer->wait(
        wait_dur,
        mainStrand->wrap(
            std::tr1::bind(&Context::shutdown, this)
        )
    );
}

void Context::run(uint32 nthreads, ExecutionThreads exthreads) {
    CTX_LOG(info, "Starting context execution with " << nthreads << " threads");

    mExecutionThreadsType = exthreads;

    uint32 nworkers = (exthreads == IncludeOriginal ? nthreads-1 : nthreads);
    // Start workers
    for(uint32 i = 0; i < nworkers; i++) {
        mWorkerThreads.push_back(
            new Thread( name + " Worker " + boost::lexical_cast<String>(i), std::tr1::bind(&Context::workerThread, this) )
        );
    }

    // Run
    if (exthreads == IncludeOriginal) {
        ioService->run();
        cleanupWorkerThreads();

        // If we exited gracefully, call shutdown automatically to clean everything
        // up, make sure stop() methods get called, etc.
        if (!mStopRequested.read())
           this->shutdown();
    }
}

void Context::workerThread() {
    ioService->run();
}

void Context::cleanupWorkerThreads() {
    // Wait for workers to finish
    for(uint32 i = 0; i < mWorkerThreads.size(); i++) {
        mWorkerThreads[i]->join();
        delete mWorkerThreads[i];
    }
    mWorkerThreads.clear();
}

void Context::shutdown() {
    CTX_LOG(info, "Handling shutdown request");

    // If the original thread wasn't running this context as well, then it won't
    // be able to wait for the worker threads it created.
    if (mExecutionThreadsType != IncludeOriginal)
        cleanupWorkerThreads();

    Signal::unregisterHandler(mSignalHandler);

    this->stop();
    for(std::vector<Service*>::iterator it = mServices.begin(); it != mServices.end(); it++)
        (*it)->stop();
}

void Context::stop() {
    if (!mStopRequested.read()) {
        mStopRequested = true;
        mFinishedTimer.reset();
        startForceQuitTimer();
    }
}


void Context::handleSignal(Signal::Type stype) {
    CTX_LOG(info, "Requesting shutdown in response to " << Signal::typeAsString(stype));
    // Try to keep this minimal. Post the shutdown process rather than
    // actually running it here. This makes the extent of the signal
    // handling known completely in this method, whereas calling
    // shutdown can cause a cascade of cleanup.
    ioService->post( std::tr1::bind(&Context::shutdown, this), "Context::shutdown" );
}

void Context::cleanup() {
    Network::IOTimerPtr timer = mKillTimer;

    if (timer) {
        timer->cancel();
        mKillTimer.reset();
        timer.reset();

        mKillService->stop();

        mKillThread->join();

        delete mKillService;
        mKillService = NULL;
        mKillThread.reset();
    }
}

void Context::startForceQuitTimer() {
    // Note that we need to do this on another thread, with another IOService.
    // This is necessary to ensure that *this* doesn't keep things from
    // exiting.
    mKillService = new Network::IOService("Context Kill Service");
    mKillTimer = Network::IOTimer::create(mKillService);
    mKillTimer->wait(
        Duration::seconds(5),
        std::tr1::bind(&Context::forceQuit, this)
    );
    mKillThread = std::tr1::shared_ptr<Thread>(
        new Thread(
            "Context Kill Thread",
            std::tr1::bind(&Network::IOService::runNoReturn, mKillService)
        )
    );
}

namespace {
void commandShutdown(Context* ctx, const Command::Command& cmd, Command::Commander* cmdr, Command::CommandID cmdid) {
    Command::Result result = Command::EmptyResult();
    cmdr->result(cmdid, result);
    ctx->shutdown();
}
}

void Context::setCommander(Command::Commander* c) {
    if (mCommander != NULL) {
        mCommander->unregisterCommand("context.shutdown");
        mCommander->unregisterCommand("context.report-stats");
        mCommander->unregisterCommand("context.report-all-stats");
    }

    mCommander = c;

    if (mCommander != NULL) {
        mCommander->registerCommand(
            "context.shutdown",
            std::tr1::bind(commandShutdown, this, _1, _2, _3)
        );

        mCommander->registerCommand(
            "context.report-stats",
            std::tr1::bind(&Network::IOService::commandReportStats, ioService, _1, _2, _3)
        );
        mCommander->registerCommand(
            "context.report-all-stats",
            std::tr1::bind(&Network::IOService::commandReportAllStats, _1, _2, _3)
        );
    }
}

} // namespace Sirikata

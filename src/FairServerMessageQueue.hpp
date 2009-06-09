#ifndef _CBR_FAIRSENDQUEUE_HPP
#define _CBR_FAIRSENDQUEUE_HPP

#include "FairQueue.hpp"
#include "ServerMessagePair.hpp"
#include "ServerMessageQueue.hpp"
#include "NetworkQueueWrapper.hpp"

namespace CBR {
class FairServerMessageQueue:public ServerMessageQueue {
protected:

    /** Predicate for FairQueue which checks if the network will be able to send the message. */
    struct CanSendPredicate {
    public:
        CanSendPredicate(FairServerMessageQueue* _fq) : fq(_fq) {}
        bool operator()(const ServerID& key, const ServerMessagePair* msg) const;
    private:
        FairServerMessageQueue* fq;
    };

    FairQueue<ServerMessagePair, ServerID, Queue<ServerMessagePair*>, CanSendPredicate > mServerQueues;
    FairQueue<ServerMessagePair, ServerID, NetworkQueueWrapper > mReceiveQueues;

    Time mLastTime;
    uint32 mRate;
    uint32 mRecvRate;
    uint32 mRemainderSendBytes;
    uint32 mRemainderReceiveBytes;
    Time mLastSendEndTime; // last packet send finish time, if there are still messages waiting
    Time mLastReceiveEndTime; // last packet receive finish time, if there are still messages waiting

    typedef std::set<ServerID> ReceiveServerSet;
    ReceiveServerSet mReceiveSet;
    struct ChunkSourcePair {
        Network::Chunk* chunk;
        ServerID source;
    };
    std::queue<ChunkSourcePair> mReceiveQueue;
public:

    FairServerMessageQueue(Network*net, uint32 send_bytes_per_second, uint32 recv_bytes_per_second, const ServerID& sid, ServerIDMap* sidmap, Trace* trace);

    virtual bool addMessage(ServerID destinationServer,const Network::Chunk&msg);
    virtual bool receive(Network::Chunk** chunk_out, ServerID* source_server_out);
    virtual void service(const Time&t);

    virtual void setServerWeight(ServerID, float weight);

    virtual void reportQueueInfo(const Time& t) const;

    virtual void getQueueInfo(std::vector<QueueInfo>& queue_info) const;

protected:
    float getServerWeight(ServerID);

    virtual void aggregateLocationMessages() { }

    // Checks if sending the given message would be successful.
    bool canSend(const ServerMessagePair* next_msg);
};
}
#endif

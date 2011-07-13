/*  Sirikata
 *  BillboardSystem.hpp
 *
 *  Copyright (c) 2011, Stanford University
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

#ifndef _SIRIKATA_MESH_BILLBOARD_SYSTEM_
#define _SIRIKATA_MESH_BILLBOARD_SYSTEM_

#include <sirikata/mesh/ModelsSystem.hpp>

namespace Sirikata {

class BillboardSystem : public ModelsSystem {
public:
    static ModelsSystem* create(const String& args);

    BillboardSystem();
    virtual ~BillboardSystem();

    // ModelsSystem Interface
    virtual bool canLoad(Transfer::DenseDataPtr data);
    virtual Mesh::MeshdataPtr load(const Transfer::URI& uri, const Transfer::Fingerprint& fp,
        Transfer::DenseDataPtr data);
    virtual bool convertMeshdata(const Mesh::Meshdata& meshdata, const String& format, const String& filename);
};

} // namespace Sirikata

#endif // _SIRIKATA_MESH_BILLBOARD_SYSTEM_
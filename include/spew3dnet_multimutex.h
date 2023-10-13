/* Copyright (c) 2020-2023, ellie/@ell1e & Spew3D Net Team (see AUTHORS.md).

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

Alternatively, at your option, this file is offered under the Apache 2
license, see accompanied LICENSE.md.
*/

#ifndef SPEW3DNET_MULTIMUTEX_H_
#define SPEW3DNET_MULTIMUTEX_H_

typedef struct s3d_mulmutex s3d_mulmutex;
typedef uint64_t s3d_mulmutexentry_t;

S3DEXP s3d_mulmutex *s3d_mulmutex_New();

S3DEXP int s3d_mulmutex_AddEntry(
    s3d_mulmutex *mm, s3d_mulmutexentry_t *out_entry
);

S3DEXP int s3d_mulmutex_DelEntry(
    s3d_mulmutex *mm, s3d_mulmutexentry_t entry
);

S3DEXP int s3d_mulmutex_IsEntryLocked(
    s3d_mulmutex *mm, s3d_mulmutexentry_t entry
);

S3DEXP void s3d_mulmutex_LockEntry(
    s3d_mulmutex *mm, s3d_mulmutexentry_t entry
);

S3DEXP int s3d_mulmutex_UnlockEntry(
    s3d_mulmutex *mm, s3d_mulmutexentry_t entry
);

S3DEXP void s3d_mulmutex_WaitForAnyUnlocked(
    s3d_mulmutex *mm
);

S3DEXP void s3d_mulmutex_UnregisterLockEntryWaiter(
    s3d_mulmutex *mm, s3d_mulmutexentry_t entry,
    void *ptrinfo
);

S3DEXP int s3d_mulmutex_RegisterLockEntryWaiter(
    s3d_mulmutex *mm, s3d_mulmutexentry_t entry,
    void *ptrinfo
);

S3DEXP s3d_mulmutexentry_t *
    s3d_mulmutex_GetNewlyMaybeUnlockedEntries(
        s3d_mulmutex *mm, uint64_t *count
    );

S3DEXP void **s3d_mulmutex_GetRegisteredWaitersForEntry(
    s3d_mulmutex *mm,
    s3d_mulmutexentry_t entry,
    uint64_t *count
);

S3DEXP void s3d_mulmutex_Destroy(s3d_mulmutex *mm);

#endif  // SPEW3DNET_MULTIMUTEX_H_


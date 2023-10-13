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

#ifdef SPEW3DNET_IMPLEMENTATION

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "s3d_mulmutex.h"

typedef struct mmstateinfo {
    void **waiter_list;
    uint64_t waiter_list_alloc, waiter_list_inuse;
} mmstateinfo;

typedef struct s3d_mulmutex {
    uint64_t state_set_alloc, state_set_inuse;
    char *state_set;
    mmstateinfo *state_set_info;

    s3d_mutex *access_lock;
    s3d_tevent *unlock_event;

    s3d_mulmutexentry_t *entries_ready_ids;
    uint64_t entries_ready_alloc, entries_ready_inuse;
} s3d_mulmutex;

S3DEXP void s3d_mulmutex_Destroy(s3d_mulmutex *mm) {
    if (!mm) return;

    uint64_t i = 0;
    while (i < mm->state_set_alloc) {
        free(mm->state_set_info[i].waiter_list);
        i++;
    }
    free(mm->state_set);
    free(mm->state_set_info);
    mutex_Destroy(mm->access_lock);
    threadevent_Free(mm->unlock_event);
    free(mm->entries_ready_ids);
    free(mm);
}

S3DEXP void **s3d_mulmutex_GetRegisteredWaitersForEntry(
        s3d_mulmutex *mm, s3d_mulmutexentry_t entry,
        uint64_t *count
        ) {
    mutex_Lock(mm->access_lock);
    assert(entry >= 0 && entry < mm->state_set_inuse);
    void **result = mm->state_set_info[entry].waiter_list;
    *count = mm->state_set_info[entry].waiter_list_inuse;
    if (mm->state_set_info[entry].waiter_list_inuse == 0)
        result = NULL;
    mutex_Release(mm->access_lock);
    return result;
}

S3DEXP s3d_mulmutexentry_t *
        s3d_mulmutex_GetNewlyMaybeUnlockedEntries(
            s3d_mulmutex *mm, uint64_t *count
        ) {
    mutex_Lock(mm->access_lock);
    s3d_mulmutexentry_t *result = NULL;
    *count = mm->entries_ready_inuse;
    if (mm->entries_ready_inuse > 0) {
        result = mm->entries_ready_ids;
    }
    mutex_Release(mm->access_lock);
    return result;
}

S3DEXP s3d_mulmutex *s3d_mulmutex_New() {
    s3d_mulmutex *mm = malloc(sizeof(*mm));
    if (!mm) {
        return NULL;
    }
    memset(mm, 0, sizeof(*mm));
    mm->access_lock = mutex_Create();
    if (!mm->access_lock) {
        free(mm);
        return NULL;
    }
    mm->unlock_event = threadevent_Create();
    if (!mm->unlock_event) {
        mutex_Destroy(mm->access_lock);
        free(mm);
        return NULL;
    }
    return mm;
}

/** This function adds an entry to the ->entries_ready_ids,
 *  indicating the mutex entry was signaled as ready for all the
 *  registered waiters. */
static int _AddReadyEntryWithoutLock(
        s3d_mulmutex *mm, s3d_mulmutexentry_t e
        ) {
    if (mm->entries_ready_inuse + 1 > mm->entries_ready_alloc) {
        uint64_t new_alloc = mm->entries_ready_alloc * 2;
        if (new_alloc < mm->entries_ready_inuse + 8) {
            new_alloc = mm->entries_ready_inuse + 8;
        }
        if (new_alloc < 4)
            new_alloc = 4;
        s3d_mulmutexentry_t *new_entries_ready = realloc(
            mm->entries_ready_ids,
            sizeof(*new_entries_ready) * new_alloc
        );
        if (!new_entries_ready) {
            return 0;
        }
        mm->entries_ready_ids = new_entries_ready;
        mm->entries_ready_alloc = new_alloc;
    }
    mm->entries_ready_inuse++;
    mm->entries_ready_ids[
        mm->entries_ready_inuse - 1
    ] = e;
    return 1;
}

S3DEXP int s3d_mulmutex_RegisterLockEntryWaiter(
        s3d_mulmutex *mm, s3d_mulmutexentry_t entry,
        void *ptrinfo
        ) {
    assert(mm != NULL);
    mutex_Lock(mm->access_lock);
    assert(entry >= 0 && entry < mm->state_set_inuse);
    mmstateinfo *sinfo = &mm->state_set_info[entry];
    if (sinfo->waiter_list_inuse + 1 > sinfo->waiter_list_alloc) {
        uint64_t new_alloc = sinfo->waiter_list_alloc * 2;
        if (new_alloc < sinfo->waiter_list_inuse + 2) {
            new_alloc = sinfo->waiter_list_inuse + 2;
        }
        if (new_alloc < 2)
            new_alloc = 2;
        void **new_waiter_list = realloc(
            sinfo->waiter_list,
            sizeof(*sinfo->waiter_list) * new_alloc
        );
        if (!new_waiter_list) {
            mutex_Release(mm->access_lock);
            return 0;
        }
        sinfo->waiter_list = new_waiter_list;
        sinfo->waiter_list_alloc = new_alloc;
    }
    sinfo->waiter_list_inuse++;
    sinfo->waiter_list[sinfo->waiter_list_inuse - 1] = (
        ptrinfo
    );
    mutex_Release(mm->access_lock);
    return 1;
}

S3DEXP void s3d_mulmutex_UnregisterLockEntryWaiter(
        s3d_mulmutex *mm, s3d_mulmutexentry_t entry,
        void *ptrinfo
        ) {
    assert(mm != NULL);
    mutex_Lock(mm->access_lock);
    assert(entry >= 0 && entry < mm->state_set_inuse);
    mmstateinfo *sinfo = &mm->state_set_info[entry];
    uint64_t i = 0;
    while (i < sinfo->waiter_list_inuse) {
        if (sinfo->waiter_list[i] == ptrinfo) {
            if (i + 1 < sinfo->waiter_list_inuse)
                memmove(
                    &sinfo->waiter_list[i],
                    &sinfo->waiter_list[i + 1],
                    (sinfo->waiter_list_inuse - i - 1) *
                        sizeof(*sinfo->waiter_list)
                );
            sinfo->waiter_list_inuse--;
        }
        i++;
    }
    mutex_Release(mm->access_lock);
}

S3DEXP void s3d_mulmutex_WaitForAnyUnlocked(
        s3d_mulmutex *mm
        ) {
    mutex_Lock(mm->access_lock);
    while (1) {
        // Check if any new ones are unlocked:
        uint64_t i = 0;
        while (i < mm->entries_ready_inuse) {
            if (mm->state_set[mm->entries_ready_ids[i]] == 1) {
                mutex_Release(mm->access_lock);
            }
            i += 1;
        }
        mm->entries_ready_inuse = 0;

        // Wait for event to be posted for new unlocks:
        mutex_Release(mm->access_lock);
        threadevent_Wait(mm->unlock_event);
        mutex_Lock(mm->access_lock);
    }
    mutex_Release(mm->access_lock);
}

S3DEXP int s3d_mulmutex_UnlockEntry(
        s3d_mulmutex *mm, s3d_mulmutexentry_t entry
        ) {
    mutex_Lock(mm->access_lock);
    assert(entry >= 0 && entry < mm->state_set_inuse);
    assert(mm->state_set[entry] == 0);
    if (!_AddReadyEntryWithoutLock(mm, entry)) {
        return 0;
    }
    mm->state_set[entry] = 1;
    threadevent_Set(mm->unlock_event);
    mutex_Release(mm->access_lock);
}

S3DEXP int s3d_mulmutex_IsEntryLocked(
        s3d_mulmutex *mm, s3d_mulmutexentry_t entry
        ) {
    mutex_Lock(mm->access_lock);
    assert(entry >= 0 && entry < mm->state_set_inuse);
    int is_locked = (mm->state_set[entry] == 0);
    mutex_Release(mm->access_lock);
    return is_locked;
}

S3DEXP void s3d_mulmutex_LockEntry(
        s3d_mulmutex *mm, s3d_mulmutexentry_t entry
        ) {
    mutex_Lock(mm->access_lock);
    assert(entry >= 0 && entry < mm->state_set_inuse);
    assert(mm->state_set[entry] == 1);
    mm->state_set[entry] = 0;
    mutex_Release(mm->access_lock);
}

S3DEXP int s3d_mulmutex_DelEntry(
        s3d_mulmutex *mm, s3d_mulmutexentry_t entry
        ) {
    mutex_Lock(mm->access_lock);
    assert(entry >= 0 && entry < mm->state_set_inuse);
    mm->state_set[entry] = 2;
    mm->state_set_info[entry].waiter_list_inuse = 0;
    if (mm->state_set_info[entry].waiter_list_alloc > 32) {
        // That's kind of large, better to throw it away.
        free(mm->state_set_info[entry].waiter_list);
        mm->state_set_info[entry].waiter_list = NULL;
        mm->state_set_info[entry].waiter_list_alloc = 0;
    }
    while (mm->state_set_inuse > 0 &&
            mm->state_set[mm->state_set_inuse - 1] == 2)
        mm->state_set_inuse--;
    mutex_Release(mm->access_lock);
}

S3DEXP int s3d_mulmutex_AddEntry(
        s3d_mulmutex *mm, s3d_mulmutexentry_t *out_entry
        ) {
    mutex_Lock(mm->access_lock);
    if (mm->state_set_inuse + 1 > mm->state_set_alloc) {
        uint64_t newstate_set_alloc = mm->state_set_alloc * 2;
        if (newstate_set_alloc < mm->state_set_inuse + 8) {
            newstate_set_alloc = mm->state_set_inuse + 8;
        }
        if (newstate_set_alloc < 16) {
            newstate_set_alloc = 16;
        }
        char *newstateset = realloc(mm->state_set,
            newstate_set_alloc * sizeof(*newstateset));
        if (!newstateset) {
            mutex_Release(mm->access_lock);
            return 0;
        }
        mm->state_set = newstateset;
        mmstateinfo *newstateinfo = realloc(
            mm->state_set_info,
            newstate_set_alloc * sizeof(*newstateinfo)
        );
        if (!newstateinfo) {
            mutex_Release(mm->access_lock);
            return 0;
        }
        mm->state_set_info = newstateinfo;
        memset(&mm->state_set_info[mm->state_set_alloc],
            0, sizeof(*mm->state_set_info) *
            (newstate_set_alloc - mm->state_set_alloc));
        mm->state_set_alloc = newstate_set_alloc;
    }
    mm->state_set_inuse++;
    *out_entry = mm->state_set_inuse - 1;
    mm->state_set[mm->state_set_inuse - 1] = 1;
    mm->state_set_info[mm->state_set_inuse - 1].waiter_list_inuse = 0;
    mutex_Release(mm->access_lock);
    return 1;
}

#endif  // SPEW3DNET_IMPLEMENTATION
 

/*
 * Copyright (c) 1998, 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#if defined(_ALLBSD_SOURCE)
#include <stdint.h>                     /* for uintptr_t */
#endif

#include "util.h"
#include "commonRef.h"

/*
 * ANDROID-CHANGED: This was modified for android to avoid any use of weak
 * global (jweak) references. On Android hosts the number of jweak
 * references active at any one time is limited. By using jweaks to keep
 * track of objects here we could hit the jweak limit on some very large
 * apps. The implementation is compatible with any JVMTI implementation
 * that provides the 'can_tag_objects' and
 * 'can_generate_object_free_events' capabilities. This works by watching
 * for the ObjectFree events on tagged objects and storing them in a list
 * of things that have been deleted.
 *
 * Each object sent to the front end is tracked with the RefNode struct
 * (see util.h).
 * External to this module, objects are identified by a jlong id which is
 * simply the sequence number. A JVMTI tag is usually used so that
 * the presence of a debugger-tracked object will not prevent
 * its collection. Once an object is collected, its RefNode may be
 * deleted (these may happen in * either order). Using the sequence number
 * as the object id prevents ambiguity in the object id when the weak ref
 * is reused. The RefNode* is stored with the object as it's JVMTI Tag.
 * This tag also provides the weak-reference behavior.
 *
 * The ref member is changed from weak to strong when gc of the object is
 * to be prevented. Whether or not it is strong, it is never exported
 * from this module.
 *
 * A reference count of each jobject is also maintained here. It tracks
 * the number times an object has been referenced through
 * commonRef_refToID. A RefNode is freed once the reference
 * count is decremented to 0 (with commonRef_release*), even if the
 * corresponding object has not been collected.
 *
 * One hash table is maintained. The mapping of ID to RefNode* is handled
 * with one hash table that will re-size itself as the number of RefNode's
 * grow.
 */

/* Initial hash table size (must be power of 2) */
#define HASH_INIT_SIZE 512
/* If element count exceeds HASH_EXPAND_SCALE*hash_size we expand & re-hash */
#define HASH_EXPAND_SCALE 8
/* Maximum hash table size (must be power of 2) */
#define HASH_MAX_SIZE  (1024*HASH_INIT_SIZE)

/* Map a key (ID) to a hash bucket */
static jint
hashBucket(jlong key)
{
    /* Size should always be a power of 2, use mask instead of mod operator */
    /*LINTED*/
    return ((jint)key) & (gdata->objectsByIDsize-1);
}

/* Generate a new ID */
static jlong
newSeqNum(void)
{
    return gdata->nextSeqNum++;
}

/* ANDROID-CHANGED: This helper function is unique to android.
 * This function gets a local-ref to object the node is pointing to. If the node's object has been
 * collected it will return NULL. The caller is responsible for calling env->DeleteLocalRef or
 * env->PopLocalFrame to clean up the reference. This function makes no changes to the passed in
 * node.
 */
static jobject
getLocalRef(JNIEnv *env, const RefNode* node) {
    if (node->isStrong) {
        return JNI_FUNC_PTR(env,NewLocalRef)(env, node->ref);
    }
    jint count = -1;
    jobject *objects = NULL;
    jlong tag = ptr_to_jlong(node);
    jvmtiError error = JVMTI_FUNC_PTR(gdata->jvmti,GetObjectsWithTags)
            (gdata->jvmti, 1, &tag, &count, &objects, NULL);
    if (error != JVMTI_ERROR_NONE) {
        EXIT_ERROR(error,"GetObjectsWithTags");
    }
    if (count != 1 && count != 0) {
        EXIT_ERROR(AGENT_ERROR_INTERNAL,
                   "GetObjectsWithTags returned multiple objects unexpectedly");
    }
    jobject res = (count == 0) ? NULL : objects[0];
    JVMTI_FUNC_PTR(gdata->jvmti,Deallocate)(gdata->jvmti,(unsigned char*)objects);
    return res;
}

/* ANDROID-CHANGED: Handler function for objects being freed. */
void commonRef_handleFreedObject(jlong tag) {
    RefNode* node = (RefNode*)jlong_to_ptr(tag);
    debugMonitorEnterNoSuspend(gdata->refLock); {
        // Delete the node and remove it from the hashmap.
        // If we raced with a deleteNode call and lost the next and prev will be null but we will
        // not be at the start of the bucket. This is fine.
        jint slot = hashBucket(node->seqNum);
        if (node->next != NULL ||
                node->prev != NULL ||
                gdata->objectsByID[slot] == node) {
            /* Detach from id hash table */
            if (node->prev == NULL) {
                gdata->objectsByID[slot] = node->next;
            } else {
                node->prev->next = node->next;
            }
            /* Also fixup back links. */
            if (node->next != NULL) {
                node->next->prev = node->prev;
            }
            gdata->objectsByIDcount--;
        }
        jvmtiDeallocate(node);
    } debugMonitorExit(gdata->refLock);
}

/* Create a fresh RefNode structure, and tag the object (creating a weak-ref to it).
 * ANDROID-CHANGED: The definition of RefNode was changed slightly so that node->ref is only for
 * a strong reference. For weak references we use the node as a tag on the object to keep track if
 * it.
 * ANDROID-CHANGED: ref must be a local-reference held live for the duration of this method until it
 * is fully in the objectByID map.
 */
static RefNode *
createNode(JNIEnv *env, jobject ref)
{
    RefNode   *node;
    jvmtiError error;

    if (ref == NULL) {
        return NULL;
    }

    /* Could allocate RefNode's in blocks, not sure it would help much */
    node = (RefNode*)jvmtiAllocate((int)sizeof(RefNode));
    if (node == NULL) {
        return NULL;
    }

    /* ANDROID-CHANGED: Use local reference to make sure we have a reference. We will use this
     * reference to set a tag to the node to use as a weak-reference and keep track of the ref.
     * ANDROID-CHANGED: Set node tag on the ref. This tag now functions as the weak-reference to the
     * object.
     */
    error = JVMTI_FUNC_PTR(gdata->jvmti, SetTag)(gdata->jvmti, ref, ptr_to_jlong(node));
    if ( error != JVMTI_ERROR_NONE ) {
        jvmtiDeallocate(node);
        return NULL;
    }

    /* Fill in RefNode */
    node->ref      = NULL;
    node->isStrong = JNI_FALSE;
    node->count    = 1;
    node->seqNum   = newSeqNum();

    /* Count RefNode's created */
    gdata->objectsByIDcount++;
    return node;
}

/* Delete a RefNode allocation, delete weak/global ref and clear tag */
static void
deleteNode(JNIEnv *env, RefNode *node)
{
    /* ANDROID-CHANGED: use getLocalRef to get a local reference to the node. */
    WITH_LOCAL_REFS(env, 1) {
        jobject localRef = getLocalRef(env, node);
        LOG_MISC(("Freeing %d\n", (int)node->seqNum));

        /* Detach from id hash table */
        if (node->prev == NULL) {
            gdata->objectsByID[hashBucket(node->seqNum)] = node->next;
        } else {
            node->prev->next = node->next;
        }
        /* Also fixup back links. */
        if (node->next != NULL) {
            node->next->prev = node->prev;
        }

        // If we don't get the localref that means the ObjectFree event is being called and the
        // node will be deleted there.
        if ( localRef != NULL ) {
            /* Clear tag */
            (void)JVMTI_FUNC_PTR(gdata->jvmti,SetTag)
                                (gdata->jvmti, localRef, NULL_OBJECT_ID);
            if (node->isStrong) {
                JNI_FUNC_PTR(env,DeleteGlobalRef)(env, node->ref);
            }

            jvmtiDeallocate(node);
        } else {
            // We are going to let the object-free do the final work. Mark this node as not in the
            // list with both null links but not in the bucket.
            node->prev = NULL;
            node->next = NULL;
        }
        gdata->objectsByIDcount--;
    } END_WITH_LOCAL_REFS(env);
}

/* Change a RefNode to have a strong reference */
static jobject
strengthenNode(JNIEnv *env, RefNode *node)
{
    if (!node->isStrong) {
        /* ANDROID-CHANGED: We need to find and fill in the node->ref when we strengthen a node. */
        WITH_LOCAL_REFS(env, 1) {
            /* getLocalRef will return NULL if the referent has been collected. */
            jobject localRef = getLocalRef(env, node);
            if (localRef != NULL) {
                node->ref = JNI_FUNC_PTR(env,NewGlobalRef)(env, localRef);
                if (node->ref == NULL) {
                    EXIT_ERROR(AGENT_ERROR_NULL_POINTER,"NewGlobalRef");
                }
                node->isStrong = JNI_TRUE;
            }
        } END_WITH_LOCAL_REFS(env);
    }
    return node->ref;
}

/* Change a RefNode to have a weak reference
 * ANDROID-CHANGED: This is done by deleting the strong reference. We already have a tag in
 * to the node from when we created the node. Since this is never removed we can simply delete the
 * global ref, reset node->isStrong & node->ref, and return. Since no part of this can fail we can
 * change this function to be void too.
 */
static void
weakenNode(JNIEnv *env, RefNode *node)
{
    if (node->isStrong) {
        JNI_FUNC_PTR(env,DeleteGlobalRef)(env, node->ref);
        node->ref      = NULL;
        node->isStrong = JNI_FALSE;
    }
}

/*
 * Returns the node which contains the common reference for the
 * given object. The passed reference should not be a weak reference
 * managed in the object hash table (i.e. returned by commonRef_idToRef)
 * because no sequence number checking is done.
 */
static RefNode *
findNodeByRef(JNIEnv *env, jobject ref)
{
    jvmtiError error;
    jlong      tag;

    tag   = NULL_OBJECT_ID;
    error = JVMTI_FUNC_PTR(gdata->jvmti,GetTag)(gdata->jvmti, ref, &tag);
    if ( error == JVMTI_ERROR_NONE ) {
        RefNode   *node;

        node = (RefNode*)jlong_to_ptr(tag);
        return node;
    }
    return NULL;
}

/* Locate and delete a node based on ID */
static void
deleteNodeByID(JNIEnv *env, jlong id, jint refCount)
{
    /* ANDROID-CHANGED: Rewrite for double-linked list. Also remove ALL_REFS since it's not needed
     * since the free-callback will do the work of cleaning up when an object gets collected. */
    jint     slot;
    RefNode *node;

    slot = hashBucket(id);
    node = gdata->objectsByID[slot];

    while (node != NULL) {
        if (id == node->seqNum) {
            node->count -= refCount;
            if (node->count <= 0) {
                if ( node->count < 0 ) {
                    EXIT_ERROR(AGENT_ERROR_INTERNAL,"RefNode count < 0");
                }
                deleteNode(env, node);
            }
            break;
        }
        node = node->next;
    }
}

/*
 * Returns the node stored in the object hash table for the given object
 * id. The id should be a value previously returned by
 * commonRef_refToID.
 *
 *  NOTE: It is possible that a match is found here, but that the object
 *        is garbage collected by the time the caller inspects node->ref.
 *        Callers should take care using the node->ref object returned here.
 *
 */
static RefNode *
findNodeByID(JNIEnv *env, jlong id)
{
    /* ANDROID-CHANGED: Rewrite for double-linked list */
    jint     slot;
    RefNode *node;

    slot = hashBucket(id);
    node = gdata->objectsByID[slot];

    while (node != NULL) {
        if ( id == node->seqNum ) {
            if ( node->prev != NULL ) {
                /* Re-order hash list so this one is up front */
                node->prev->next = node->next;
                node->prev->prev = node->prev;
                node->next = gdata->objectsByID[slot];
                node->next->prev = node;
                node->prev = NULL;
                gdata->objectsByID[slot] = node;
            }
            break;
        }
        node = node->next;
    }
    return node;
}

/* Initialize the hash table stored in gdata area */
static void
initializeObjectsByID(int size)
{
    /* Size should always be a power of 2 */
    if ( size > HASH_MAX_SIZE ) size = HASH_MAX_SIZE;
    gdata->objectsByIDsize  = size;
    gdata->objectsByIDcount = 0;
    gdata->objectsByID      = (RefNode**)jvmtiAllocate((int)sizeof(RefNode*)*size);
    (void)memset(gdata->objectsByID, 0, (int)sizeof(RefNode*)*size);
}

/* hash in a RefNode */
static void
hashIn(RefNode *node)
{
    /* ANDROID-CHANGED: Modify for double-linked list */
    jint     slot;

    /* Add to id hashtable */
    slot                     = hashBucket(node->seqNum);
    node->next               = gdata->objectsByID[slot];
    node->prev               = NULL;
    if (node->next != NULL) {
        node->next->prev     = node;
    }
    gdata->objectsByID[slot] = node;
}

/* Allocate and add RefNode to hash table
 * ANDROID-CHANGED: Requires that ref be a held-live local ref.*/
static RefNode *
newCommonRef(JNIEnv *env, jobject ref)
{
    RefNode *node;

    /* Allocate the node and set it up */
    node = createNode(env, ref);
    if ( node == NULL ) {
        return NULL;
    }

    /* See if hash table needs expansion */
    if ( gdata->objectsByIDcount > gdata->objectsByIDsize*HASH_EXPAND_SCALE &&
         gdata->objectsByIDsize < HASH_MAX_SIZE ) {
        RefNode **old;
        int       oldsize;
        int       newsize;
        int       i;

        /* Save old information */
        old     = gdata->objectsByID;
        oldsize = gdata->objectsByIDsize;
        /* Allocate new hash table */
        gdata->objectsByID = NULL;
        newsize = oldsize*HASH_EXPAND_SCALE;
        if ( newsize > HASH_MAX_SIZE ) newsize = HASH_MAX_SIZE;
        initializeObjectsByID(newsize);
        /* Walk over old one and hash in all the RefNodes */
        for ( i = 0 ; i < oldsize ; i++ ) {
            RefNode *onode;

            onode = old[i];
            while (onode != NULL) {
                RefNode *next;

                next = onode->next;
                hashIn(onode);
                onode = next;
            }
        }
        jvmtiDeallocate(old);
    }

    /* Add to id hashtable */
    hashIn(node);
    return node;
}

/* Initialize the commonRefs usage */
void
commonRef_initialize(void)
{
    gdata->refLock = debugMonitorCreate("JDWP Reference Table Monitor");
    gdata->nextSeqNum       = 1; /* 0 used for error indication */
    initializeObjectsByID(HASH_INIT_SIZE);
}

/* Reset the commonRefs usage */
void
commonRef_reset(JNIEnv *env)
{
    debugMonitorEnter(gdata->refLock); {
        int i;

        for (i = 0; i < gdata->objectsByIDsize; i++) {
            RefNode *node;

            for (node = gdata->objectsByID[i]; node != NULL; node = gdata->objectsByID[i]) {
                deleteNode(env, node);
            }
            gdata->objectsByID[i] = NULL;
        }

        /* Toss entire hash table and re-create a new one */
        jvmtiDeallocate(gdata->objectsByID);
        gdata->objectsByID      = NULL;
        gdata->nextSeqNum       = 1; /* 0 used for error indication */
        initializeObjectsByID(HASH_INIT_SIZE);

    } debugMonitorExit(gdata->refLock);
}

/*
 * Given a reference obtained from JNI or JVMTI, return an object
 * id suitable for sending to the debugger front end.
 */
jlong
commonRef_refToID(JNIEnv *env, jobject ref)
{
    jlong id;

    if (ref == NULL) {
        return NULL_OBJECT_ID;
    }

    id = NULL_OBJECT_ID;
    debugMonitorEnter(gdata->refLock); {
        RefNode *node;

        node = findNodeByRef(env, ref);
        if (node == NULL) {
            WITH_LOCAL_REFS(env, 1) {
                node = newCommonRef(env, JNI_FUNC_PTR(env,NewLocalRef)(env, ref));
                if ( node != NULL ) {
                    id = node->seqNum;
                }
            } END_WITH_LOCAL_REFS(env);
        } else {
            id = node->seqNum;
            node->count++;
        }
    } debugMonitorExit(gdata->refLock);
    return id;
}

/*
 * Given an object ID obtained from the debugger front end, return a
 * strong, global reference to that object (or NULL if the object
 * has been collected). The reference can then be used for JNI and
 * JVMTI calls. Caller is resposible for deleting the returned reference.
 */
jobject
commonRef_idToRef(JNIEnv *env, jlong id)
{
    jobject ref;

    ref = NULL;
    debugMonitorEnter(gdata->refLock); {
        RefNode *node;

        node = findNodeByID(env, id);
        if (node != NULL) {
            if (node->isStrong) {
                saveGlobalRef(env, node->ref, &ref);
            } else {
                jobject lref;

                /* ANDROID-CHANGED: Use getLocalRef helper to get a local-reference to the object
                 * this node weakly points to. It will return NULL if the object has been GCd
                 */
                lref = getLocalRef(env, node);
                if ( lref != NULL ) {
                    /* ANDROID-CHANGED: Use lref to save the global ref since that is the only real
                     * jobject we have.
                     */
                    saveGlobalRef(env, lref, &ref);
                    JNI_FUNC_PTR(env,DeleteLocalRef)(env, lref);
                }
                /* ANDROID-CHANGED: Otherwise the object was GC'd shortly after we found the node.
                 * The free callback will deal with cleanup once we return.
                 */
            }
        }
    } debugMonitorExit(gdata->refLock);
    return ref;
}

/* Deletes the global reference that commonRef_idToRef() created */
void
commonRef_idToRef_delete(JNIEnv *env, jobject ref)
{
    if ( ref==NULL ) {
        return;
    }
    tossGlobalRef(env, &ref);
}


/* Prevent garbage collection of an object */
jvmtiError
commonRef_pin(jlong id)
{
    jvmtiError error;

    error = JVMTI_ERROR_NONE;
    if (id == NULL_OBJECT_ID) {
        return error;
    }
    debugMonitorEnter(gdata->refLock); {
        JNIEnv  *env;
        RefNode *node;

        env  = getEnv();
        node = findNodeByID(env, id);
        if (node == NULL) {
            error = AGENT_ERROR_INVALID_OBJECT;
        } else {
            jobject strongRef;

            strongRef = strengthenNode(env, node);
            if (strongRef == NULL) {
                /*
                 * Referent has been collected, clean up now.
                 * ANDROID-CHANGED: The node will be cleaned up by the object-free callback.
                 */
                error = AGENT_ERROR_INVALID_OBJECT;
            }
        }
    } debugMonitorExit(gdata->refLock);
    return error;
}

/* Permit garbage collection of an object */
jvmtiError
commonRef_unpin(jlong id)
{
    jvmtiError error;

    error = JVMTI_ERROR_NONE;
    debugMonitorEnter(gdata->refLock); {
        JNIEnv  *env;
        RefNode *node;

        env  = getEnv();
        node = findNodeByID(env, id);
        if (node != NULL) {
            // ANDROID-CHANGED: weakenNode was changed to never fail.
            weakenNode(env, node);
        }
    } debugMonitorExit(gdata->refLock);
    return error;
}

/* Release tracking of an object by ID */
void
commonRef_release(JNIEnv *env, jlong id)
{
    debugMonitorEnter(gdata->refLock); {
        deleteNodeByID(env, id, 1);
    } debugMonitorExit(gdata->refLock);
}

void
commonRef_releaseMultiple(JNIEnv *env, jlong id, jint refCount)
{
    debugMonitorEnter(gdata->refLock); {
        deleteNodeByID(env, id, refCount);
    } debugMonitorExit(gdata->refLock);
}

/* Get rid of RefNodes for objects that no longer exist */
void
commonRef_compact(void)
{
    // NO-OP.
}

/* Lock the commonRef tables */
void
commonRef_lock(void)
{
    debugMonitorEnter(gdata->refLock);
}

/* Unlock the commonRef tables */
void
commonRef_unlock(void)
{
    debugMonitorExit(gdata->refLock);
}

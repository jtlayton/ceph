// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Tests for Ceph delegation handling
 *
 * (c) 2017, Jeff Layton <jlayton@redhat.com>
 */

#include "gtest/gtest.h"
#include "include/cephfs/libcephfs.h"
#include "include/stat.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/xattr.h>
#include <sys/uio.h>

#ifdef __linux__
#include <limits.h>
#endif

#include <map>
#include <vector>
#include <thread>
#include <atomic>

#define	CEPHFS_RECLAIM_TIMEOUT		60

TEST(LibCephFS, ReclaimSimple) {
  pid_t		pid;
  char		uuid[256];

  sprintf(uuid, "simplereclaim:%x", getpid());

  pid = fork();
  ASSERT_GE(pid, 0);
  if (pid == 0) {
    /* child - mount, acquire state and let it die */
    struct ceph_mount_info *cmount;
    ASSERT_EQ(ceph_create(&cmount, NULL), 0);
    ASSERT_EQ(ceph_conf_read_file(cmount, NULL), 0);
    ASSERT_EQ(0, ceph_conf_parse_env(cmount, NULL));
    ASSERT_EQ(ceph_init(cmount), 0);
    ceph_set_uuid(cmount, uuid);
    ceph_set_timeout(cmount, CEPHFS_RECLAIM_TIMEOUT);
    ASSERT_EQ(ceph_mount(cmount, "/"), 0);

    Inode *root, *file;
    ASSERT_EQ(ceph_ll_lookup_root(cmount, &root), 0);

    Fh *fh;
    struct ceph_statx stx;
    UserPerm *perms = ceph_mount_perms(cmount);

    ASSERT_EQ(ceph_ll_create(cmount, root, uuid, 0666,
		    O_RDWR|O_CREAT|O_EXCL, &file, &fh, &stx, 0, 0, perms), 0);
    exit(0);
  }

  /* parent - wait for child to exit */
  int ret;
  pid_t wp = wait(&ret);
  ASSERT_GE(wp, 0);
  ASSERT_EQ(WIFEXITED(ret), true);
  ASSERT_EQ(WEXITSTATUS(ret), 0);

  struct ceph_mount_info *cmount;
  ASSERT_EQ(ceph_create(&cmount, NULL), 0);
  ASSERT_EQ(ceph_conf_read_file(cmount, NULL), 0);
  ASSERT_EQ(0, ceph_conf_parse_env(cmount, NULL));
  ASSERT_EQ(ceph_init(cmount), 0);
  ceph_set_timeout(cmount, CEPHFS_RECLAIM_TIMEOUT);
  ASSERT_EQ(ceph_start_reclaim(cmount, uuid, 0), 0);
  ceph_set_uuid(cmount, uuid);
  ASSERT_EQ(ceph_mount(cmount, "/"), 0);

  Inode *root, *file;
  ASSERT_EQ(ceph_ll_lookup_root(cmount, &root), 0);
  UserPerm *perms = ceph_mount_perms(cmount);
  struct ceph_statx stx;
  ASSERT_EQ(ceph_ll_lookup(cmount, root, uuid, &file, &stx, 0, 0, perms), 0);
  Fh *fh;
  ASSERT_EQ(ceph_ll_open(cmount, file, O_WRONLY, &fh, perms), 0);

  ceph_unmount(cmount);
  ceph_release(cmount);
}

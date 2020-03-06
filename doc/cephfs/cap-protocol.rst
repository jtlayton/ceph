============================
CephFS Capabilities Protocol
============================
Many of the calls that a cephfs client makes to the MDS are fairly
straightforward for anyone familiar with network filesystems (NFS,
SMB, 9P, et. al.).

CEPH_MSG_CLIENT_CAPS messages deserve special mention as they are
used by the client and MDS to maintain a shared cache of capabilities
across both entities.

Header
======
Caps messages are normal ceph MDS messages that carry a payload of
a struct ceph_mds_caps_head and ceph_mds_caps_body. Like most Ceph
calls, they have a common format::

        struct ceph_mds_caps_head {
                __le32 op;                  /* CEPH_CAP_OP_* */
                __le64 ino, realm;
                __le64 cap_id;
                __le32 seq, issue_seq;
                __le32 caps, wanted, dirty; /* latest issued/wanted/dirty */
                __le32 migrate_seq;
                __le64 snap_follows;
                __le32 snap_trace_len;

                /* authlock */
                __le32 uid, gid, mode;

                /* linklock */
                __le32 nlink;

                /* xattrlock */
                __le32 xattr_len;
                __le64 xattr_version;
        } __attribute__ ((packed));

Most messages have the same format, aside from the EXPORT message
In the Linux kernel sources, these are mostly mirrored by
struct ceph_mds_caps. The kernel hand decodes the body section
in order to deal with EXPORT messages.

Each of these fields expresses the sending entity's current understanding
of the state of the caps cache. This is a declarative sort of protocol,
where the client and MDS modify their internal state to match the consensus
view between the two.

Some basic descriptions of the fields:

op
  The operation being requested

ino + realm (snapid?)
  Identifies inode that this request concerns

cap_id
  Unique identifier for this cap record. Assigned by the MDS, opaque to client.

caps:
  Caps currently issued by the MDS (expressed as bitmask. See
  :doc:`cephfs/capabilities`)

wanted:
  Caps currently wanted by the client

dirty:
  Dirty caps. When inode metadata is modified under the aegis of an exclusive
  cap, then this tells the MDS which fields to update.

seq
  Incremented by the MDS whenever it issues an updated set of caps.

issue_seq
  Mostly used in cap IMPORT/EXPORT (?)

migrate_seq
  Sequence at the time of Cap IMPORT/EXPORT?

snap_follows
  Snapid of ?

snap_trace_len
  Length of the trailing snaptrace blob.

Body
====
After the head comes a body section. Note that it is a union. EXPORT
messages use the peer field, but all other calls use the unnamed
struct::

        /* extra info for cap import/export */
        struct ceph_mds_cap_peer {
                __le64 cap_id;
                __le32 seq;
                __le32 mseq;
                __le32 mds;
                __u8   flags;
        } __attribute__ ((packed));

        struct ceph_mds_caps_body_legacy {
               union {
                        /* all except export */
                        struct {
                                /* filelock */
                                __le64 size, max_size, truncate_size;
                                __le32 truncate_seq;
                                struct ceph_timespec mtime, atime, ctime;
                                struct ceph_file_layout layout;
                                __le32 time_warp_seq;
                        } __attribute__ ((packed));
                        /* export message */
                        struct ceph_mds_cap_peer peer;
                } __attribute__ ((packed));
        } __attribute__ ((packed));

Operations
==========

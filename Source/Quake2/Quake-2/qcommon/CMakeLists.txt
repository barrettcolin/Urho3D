set(qcommon_CPP_FILES
  cmd.c
  cmodel.c
  common.c
  crc.c
  cvar.c
  files.c
  md4.c
  net_chan.c
  pmove.c)

set(qcommon_H_FILES
  qcommon.h
  qfiles.h)

add_library(q2common ${qcommon_CPP_FILES} ${qcommon_H_FILES})

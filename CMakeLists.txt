cmake_minimum_required(VERSION 3.15)
project(DataBase)

set(CMAKE_CXX_STANDARD 14)

set(
        PF_MODULE
        PF/pf_hashtable.h
        PF/pf_buffermgr.h
        PF/pf.h
        PF/pf_internal.h
        PF/pf_statistics.cc
        PF/pf_pagehandle.cc
        PF/pf_manager.cc
        PF/pf_hashtable.cc
        PF/pf_filehandle.cc
        PF/pf_error.cc
        PF/pf_buffermgr.cc
)
set(
        RM_MODULE
        RM/rm.h
        RM/rm_manager.h
        RM/rm_bitmap.h
        RM/rm_filehandle.h
        RM/rm_record.h
        RM/rm_rid.h
        RM/rm_bitmap.cc
        RM/rm_error.cc
        RM/rm_filehandle.cc
        RM/rm_filescan.cc
        RM/rm_manager.cc
        RM/rm_record.cc
        RM/rm_rid.cc
)
set(
        IX_MODULE
        IX/ix_formation.h
        IX/ix_indexhandle.h
        IX/ix_indexscan.h
        IX/ix_manager.h
        IX/ix.h
        IX/ix_error.h
        IX/ix_indexhandle.cc
        IX/ix_indexscan.cc
        IX/ix_manager.cc
        IX/comparators.h
        IX/indexheader.h
)

set(
        SM_MODULE
        SM/linkedlist.h
        SM/sm.h
        SM/sm_catalog.h
        SM/sm_error.cc
        SM/sm_manager.cc
        SM/sm_manager.h
        SM/sm_printerror.h
        SM/sm_scan.cc
        SM/sm_scan.h
        SM/statistics.cc
        SM/statistics.h
        SM/printer.h
        SM/printer.cc
)

set(
        PARSER_MODULE
        PARSER/interp.cc
        PARSER/nodes.cc
        PARSER/parse.cc
        PARSER/parser.h
        PARSER/parser_internal.h
        PARSER/scanhelp.cc
        PARSER/y.tab.h
)


add_executable(DataBase redbase.h main.cpp ${PF_MODULE} ${RM_MODULE} ${IX_MODULE} ${SM_MODULE} ${PARSER_MODULE})
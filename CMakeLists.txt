cmake_minimum_required(VERSION 3.15)
project(DataBase)

set(CMAKE_CXX_STANDARD 14)

set(
        HEADER_FILES
        ./Header/pf_hashtable.h
        ./Header/pf_buffermgr.h
        ./Header/pf.h
        ./Header/pf_internal.h
        ./Header/redbase.h
        ./Header/rm.h
        ./Header/rm_manager.h
        ./Header/rm_bitmap.h
        ./Header/rm_filehandle.h
        ./Header/rm_record.h
        ./Header/rm_rid.h
)

set(
        SOURCE_FILES
        ./Source/pf_statistics.cc
        ./Source/pf_pagehandle.cc
        ./Source/pf_manager.cc
        ./Source/pf_hashtable.cc
        ./Source/pf_filehandle.cc
        ./Source/pf_error.cc
        ./Source/pf_buffermgr.cc
        ./Source/rm_bitmap.cc
        ./Source/rm_error.cc
        ./Source/rm_filehandle.cc
        ./Source/rm_filescan.cc
        ./Source/rm_manager.cc
        ./Source/rm_record.cc
        ./Source/rm_rid.cc
)

add_executable(DataBase main.cpp ${HEADER_FILES} ${SOURCE_FILES})
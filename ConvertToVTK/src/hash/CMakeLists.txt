set(SOURCE_FILES_HASH hash_common.h
        point_hash.cpp
        point_hash.h
        )


ADD_LIBRARY(hashes STATIC ${SOURCE_FILES_HASH})

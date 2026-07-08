qualcomm_proto — Qualcomm DIAG protocol shared library
=======================================================

What this is
------------
Shared C++ utilities for the Qualcomm DIAG protocol layer.
No Python dependency. Consumed by two independent modules:

  monitor_c/      uses log_config + consts to send SET_MASK commands to modem
  dm_collector_c/ uses consts + utils to filter and identify incoming log packets

Files
-----
  utils.h / utils.cpp
      IdVector (typedef std::vector<int>)
      ValueName struct (int val, const char* name, bool b_public)
      find_ids()     -- look up type IDs by name string
      search_name()  -- look up name string by type ID
      ARRAY_SIZE()   -- macro: sizeof(array)/sizeof(element_type)

  consts.h / consts.cpp
      LogPacketType enum  -- all ~90 supported Qualcomm log packet type IDs
      LogPacketTypeID_To_Name[]  -- mapping table (int <-> name string)
      LogPacketTypeID_To_Name_n  -- number of entries in that table

      NOTE: the array is defined in consts.cpp, not in the header.
      Use LogPacketTypeID_To_Name_n instead of ARRAY_SIZE() when
      referencing this array from other translation units.

  log_config.h / log_config.cpp
      LogConfigOp enum  -- DIAG command opcodes (DISABLE, SET_MASK, etc.)
      BinaryBuffer      -- typedef pair<char*, int> (heap buffer + length)
      encode_log_config()  -- build a raw DIAG command payload
      get_equip_id()       -- extract upper 4 bits (radio tech) from type ID
      get_item_id()        -- extract lower 12 bits from type ID

Building (desktop / Linux)
--------------------------
  cd bytes_proto/qualcomm
  make

  Produces: libqualcomm_proto.a

  To use from another module's Makefile:
    CXXFLAGS += -I../bytes_proto/qualcomm
    LDFLAGS  += -L../bytes_proto/qualcomm -lqualcomm_proto

Building (Android NDK via CMake)
---------------------------------
  In the consuming module's CMakeLists.txt:

    add_subdirectory(
        ${CMAKE_SOURCE_DIR}/../bytes_proto/qualcomm
        ${CMAKE_BINARY_DIR}/qualcomm_proto_build   # separate build dir for out-of-tree path
    )
    target_link_libraries(your_target PRIVATE qualcomm_proto)

  The PUBLIC include path in qualcomm_proto's CMakeLists.txt propagates
  automatically, so no extra target_include_directories() is needed.

Why consts.h declares extern instead of defining the array
-----------------------------------------------------------
  In C++, a const array defined directly in a header has internal linkage.
  That means every .cpp that includes the header gets its own private copy
  compiled into its .o file. For a ~260-entry table of structs that is
  16 bytes each (int + pointer + bool + padding), that is ~4 KB per
  translation unit. If five files include the header you have 20 KB in the
  final binary instead of 4 KB.

  With extern in the header + one definition in consts.cpp:
    - consts.o contains the one copy (external linkage)
    - all other .o files reference the same symbol
    - the linker resolves them all to the same 4 KB at final link time

  As a consequence, sizeof(LogPacketTypeID_To_Name) is only valid inside
  consts.cpp (where the full definition is visible). Everywhere else,
  use LogPacketTypeID_To_Name_n which is exported for this purpose.
#MESSAGE( "External project - DCMTK" )

set(proj DCMTK)

set(${proj}_DEPENDENCIES "")

IF(CMAKE_CXX_STANDARD AND UNIX)
  LIST(APPEND ep_cxx_standard_args
    -DCMAKE_CXX_STANDARD:STRING=${CMAKE_CXX_STANDARD}
    -DCMAKE_CXX_STANDARD_REQUIRED:BOOL=${CMAKE_CXX_STANDARD_REQUIRED}
    -DCMAKE_CXX_EXTENSIONS:BOOL=ON
    )
ENDIF()

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED YES) 

ExternalProject_Add( 
  DCMTK
  URL ftp://dicom.offis.de/pub/dicom/offis/software/dcmtk/dcmtk363/dcmtk-3.6.3.tar.gz
  SOURCE_DIR DCMTK-source
  BINARY_DIR DCMTK-build
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  INSTALL_COMMAND cmake -E echo "Skipping install step."
  CMAKE_GENERATOR ${gen}
  CMAKE_ARGS
    ${ep_common_args}
    #${ep_cxx_standard_args}
    #-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}
    -DCMAKE_INSTALL_PREFIX:STRING=${CMAKE_BINARY_DIR}/install
    -DCMAKE_DEBUG_POSTFIX:STRING=d
    -DBUILD_APPS:BOOL=OFF
    -DBUILD_EXAMPLES:BOOL=OFF
    -DBUILD_TESTING:BOOL=OFF
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    -DDCMTK_WITH_DOXYGEN:BOOL=OFF
    -DDCMTK_WITH_ZLIB:BOOL=OFF # see github issue #25
    -DDCMTK_WITH_SNDFILE:BOOL=OFF
    -DDCMTK_WITH_OPENSSL:BOOL=OFF # see github issue #25
    -DDCMTK_WITH_PNG:BOOL=OFF # see github issue #25
    -DDCMTK_WITH_TIFF:BOOL=OFF  # see github issue #25
    -DDCMTK_WITH_XML:BOOL=OFF  # see github issue #25
    -DDCMTK_WITH_ICU:BOOL=OFF  # see github issue #178
    -DDCMTK_WITH_ICONV:BOOL=OFF  # see github issue #178
    -DDCMTK_WITH_STDLIBC_ICONV:BOOL=OFF  # see github issue #178
    -DDCMTK_ENABLE_LFS=OFF
    -DDCMTK_FORCE_FPIC_ON_UNIX:BOOL=ON
    -DDCMTK_OVERWRITE_WIN32_COMPILER_FLAGS:BOOL=OFF
    -DDCMTK_ENABLE_BUILTIN_DICTIONARY:BOOL=ON
    -DDCMTK_ENABLE_PRIVATE_TAGS:BOOL=ON
    -DDCMTK_ENABLE_CXX11:BOOL=ON 
    -DDCMTK_ENABLE_STL:BOOL=ON 
    -DDCMTK_ENABLE_CHARSET_CONVERSION:STRING=<disabled>
)

SET( DCMTK_DIR ${CMAKE_BINARY_DIR}/DCMTK-build )

LIST(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/DCMTK-build")
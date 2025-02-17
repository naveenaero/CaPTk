#MESSAGE( "External project - VTK" )

SET( VTK_DEPENDENCIES )

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED YES) 

MESSAGE( STATUS "Adding VTK-8.1.0 ...")

ExternalProject_Add( 
  VTK
  URL https://github.com/Kitware/VTK/archive/v8.1.0.zip
  #GIT_REPOSITORY ${git_protocol}://github.com/Kitware/VTK.git
  #GIT_TAG v8.1.0
  SOURCE_DIR VTK-source
  BINARY_DIR VTK-build
  UPDATE_COMMAND ""
  #BUILD_COMMAND ""
  #PATCH_COMMAND "git apply --whitespace=nowarn ${CMAKE_CURRENT_BINARY_DIR}/vtk.patch"
  CMAKE_GENERATOR ${gen}
  INSTALL_COMMAND cmake -E echo "Skipping install step."
  CMAKE_ARGS
    ${ep_common_args}
    #-DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}
    -DExternalData_OBJECT_STORES:STRING=${ExternalData_OBJECT_STORES}
    -DBUILD_EXAMPLES:BOOL=OFF # examples are not needed
    -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS} # no static builds
    -DBUILD_TESTING:BOOL=OFF 
    -DVTK_Group_Qt:BOOL=ON # [QT] dependency, enables better GUI
    -DVTK_LEGACY_SILENT:BOOL=ON # disables the QVTKWidget warning
    -DCMAKE_CXX_MP_FLAG=ON 
    -DVTK_Group_Imaging=ON 
    -DModule_vtkGUISupportQt:BOOL=ON 
    -DModule_vtkGUISupportQtOpenGL:BOOL=ON 
    -DModule_vtkRenderingQt:BOOL=ON 
    -DModule_vtkViewsQt:BOOL=ON
    -DVTK_USE_QTCHARTS:BOOL=ON
    -DVTK_USE_QVTK_QTOPENGL:BOOL=ON    
    -DCMAKE_DEBUG_POSTFIX:STRING=d
    -DVTK_QT_VERSION:STRING=5 
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:STRING=${CMAKE_INSTALL_PREFIX}
    -DQt5_DIR=${CAPTK_QT5_DIR}
)

SET( VTK_DIR ${CMAKE_BINARY_DIR}/VTK-build )
#LIST(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/VTK-build")
SET( ENV{CMAKE_PREFIX_PATH} "${CMAKE_PREFIX_PATH};${CMAKE_BINARY_DIR}/VTK-build" )

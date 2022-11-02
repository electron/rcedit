cmake_minimum_required(VERSION 3.21)

project(rcedit) 

set_property( GLOBAL PROPERTY USE_FOLDERS ON )

file( REAL_PATH ~/bin CMAKE_INSTALL_PREFIX EXPAND_TILDE)

include_directories( ${CMAKE_BINARY_DIR} )
add_executable( ${PROJECT_NAME} 
                src/main.cc
                src/rescle.cc
                src/rcedit.rc
                src/version.h
          )

INSTALL( TARGETS ${PROJECT_NAME} RUNTIME DESTINATION . )
INSTALL( FILES ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${PROJECT_NAME}.pdb DESTINATION . CONFIGURATIONS Debug RelWithDebInfo )


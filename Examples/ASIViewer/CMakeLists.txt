if(NOT TARGET simUtil)
    return()
endif()

project(EXAMPLE_ASI_VIEWER)

set(PROJECT_FILES
    ASIViewer.cpp
)

add_executable(example_asiviewer ${PROJECT_FILES})
target_link_libraries(example_asiviewer PRIVATE simVis simUtil)
set_target_properties(example_asiviewer PROPERTIES
    FOLDER "Examples"
    PROJECT_LABEL "Example - ASI Simple Viewer"
)
vsi_install_target(example_asiviewer SDK_Examples)

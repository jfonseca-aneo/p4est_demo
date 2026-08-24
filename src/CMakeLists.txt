set (P4EST_DEMO_EXEC_FILES "")

list (APPEND P4EST_DEMO_EXEC_FILES
    main_2d.cpp
    main_3d.cpp
)

foreach( execsource ${P4EST_DEMO_EXEC_FILES} )
    get_filename_component( execname ${execsource} NAME_WE )
    add_executable( ${execname} ${execsource} )
    target_link_libraries( ${execname} P4EST_DEMO )
    install( TARGETS ${execname} DESTINATION bin )
endforeach( execsource ${P4EST_DEMO_EXEC_FILES} )

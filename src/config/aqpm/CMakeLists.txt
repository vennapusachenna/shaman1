if (KDE4_INTEGRATION)
    ########### next target ###############

    set(kcm_aqpm_PART_SRCS
        AqpmConfig.cpp
        ../../ShamanDialog.cpp
        ../../ShamanIcon.cpp
    )

    kde4_add_ui_files(kcm_aqpm_PART_SRCS
        aqpmConfig.ui
    )

    qt4_add_resources (qtsourceview_RC_SRCS  ../../../ui/shaman.qrc)

    kde4_add_plugin(kcm_aqpm ${kcm_aqpm_PART_SRCS} ${qtsourceview_RC_SRCS})

    target_link_libraries(kcm_aqpm ${KDE4_KDEUI_LIBS} ${POLKITQT_LIBRARIES} ${AQPM_LIBRARY})

    install(TARGETS kcm_aqpm  DESTINATION ${PLUGIN_INSTALL_DIR} )

    ########### install files ###############

    install( FILES kcmaqpm.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

endif (KDE4_INTEGRATION)

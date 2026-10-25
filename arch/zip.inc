ifeq (${SUBARCH},)
SUBARCH := ${SUBPLATFORM}
endif

archive: build
	${RM} -r build/dist/${SUBPLATFORM}
	${MKDIR} -p build/dist/${SUBPLATFORM}
	@cd build/${SUBPLATFORM} && \
	${ZIP} "../dist/${SUBPLATFORM}/${TARGET}-${SUBARCH}.zip" *

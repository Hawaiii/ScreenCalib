################################################################################
# ExtendedShutterEx Makefile.
################################################################################
PROJECT_ROOT=.
OPT_INC = ${PROJECT_ROOT}/make/common.mk
-include ${OPT_INC}
# Handle environment variables
ifeq ($(wildcard ${OPT_INC}),)
	CXX   = g++
	ODIR  = .obj/build${D}
	SDIR  = .
	MKDIR = mkdir -p
	MV    = mv
endif

BASE_NAME = ScreenCalib
NAME      = ${BASE_NAME}${D}
INC       = -I/usr/include/flycapture
LIBS      = -L/usr/lib -lflycapture${D} ${FC2_DEPS} 
LIBS      += $(shell pkg-config --libs opencv)
OUTDIR    = ${PROJECT_ROOT}/bin
CXXFLAGS    = $(shell pkg-config --cflags opencv)
CXXFLAGS  += -std=c++11

_OBJ      = ${BASE_NAME}.o
SRC       = $(_OBJ:.o=.cpp)
OBJ       = $(patsubst %,$(ODIR)/%,$(_OBJ))

# Master rule
.PHONY: all
all: ${NAME}

# Output binary
${NAME}: ${OBJ}
	${CXX} ${OBJ} -o ${NAME} ${LIBS}
	-@${MV} ${NAME} ${OUTDIR}/${NAME}

# Intermediate object files
${OBJ}: ${ODIR}/%.o : ${SDIR}/%.cpp
	@${MKDIR} ${ODIR}
	${CXX} ${CXXFLAGS} ${LINUX_DEFINES} ${INC} -Wall -c $< -o $@

# Cleanup intermediate objects
.PHONY: clean_obj
clean_obj:
	rm -f ${OBJ}
	@echo "obj cleaned up!"

# Cleanup everything
.PHONY: clean
clean: clean_obj
	rm -f ${OUTDIR}/${NAME} ${OBJ}
	@echo "all cleaned up!"


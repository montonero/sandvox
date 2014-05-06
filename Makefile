ifeq ($(wildcard build),)
	ifeq ($(CMAKE_GENERATOR),)
		CMAKE_GENERATOR=-G Xcode
	endif
endif

all: cmake

cmake:
	@mkdir -p build
	cmake -H. -Bbuild $(CMAKE_GENERATOR)

.PHONY: cmake
# GNU Make solution makefile autogenerated by Premake
# Type "make help" for usage help

ifndef config
  config=debug
endif
export config

PROJECTS := kalei glfw glew imgui stb

.PHONY: all clean help $(PROJECTS)

all: $(PROJECTS)

kalei: glfw glew stb imgui
	@echo "==== Building main programm ===="
	@${MAKE} --no-print-directory -C . -f kalei.make

glfw: 
	@echo "==== Building glfw ===="
	@${MAKE} --no-print-directory -C . -f glfw.make

glew: 
	@echo "==== Building glew ===="
	@${MAKE} --no-print-directory -C . -f glew.make

imgui: 
	@echo "==== Building imgui ===="
	@${MAKE} --no-print-directory -C . -f imgui.make

stb: 
	@echo "==== Building stb ===="
	@${MAKE} --no-print-directory -C . -f stb.make

clean:
	@${MAKE} --no-print-directory -C . -f kalei.make clean
	@${MAKE} --no-print-directory -C . -f glfw.make clean
	@${MAKE} --no-print-directory -C . -f glew.make clean
	@${MAKE} --no-print-directory -C . -f imgui.make clean
	@${MAKE} --no-print-directory -C . -f stb.make clean

help:
	@echo "Usage: make [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "   debug"
	@echo "   release"
	@echo "   debug64"
	@echo "   release64"
	@echo "   debug32"
	@echo "   release32"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   kalei"
	@echo "   glfw"
	@echo "   glew"
	@echo "   imgui"
	@echo "   stb"
	@echo ""
	@echo "For more information, see http://industriousone.com/premake/quick-start"

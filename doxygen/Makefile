# We don't want to force the top-level Makefile to build the Doxygen docs
# for an end-user. For this, we will override the "all" target to display
# a message to this effect, then use "codedocs" as the actual build target.
#

all:
	@echo "Ignoring the 'all' target; to build the code docs, use 'make codedocs'."

codedocs:
	doxygen Doxyfile

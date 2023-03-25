#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
#

COMPONENT_ADD_INCLUDEDIRS := .
COMPONENT_SRCDIRS		  := .

								# lcd         \
								# ring_buffer \
								# knob        \

COMPONENT_ADD_INCLUDEDIRS += 	arch/esp32  \
								arch/misc   \
								gatt_server \
								wifi_net    \
								button      \
								spi         \


								# lcd         \
								# ring_buffer \
								# knob        \

COMPONENT_SRCDIRS         +=	arch/esp32  \
								arch/misc   \
								gatt_server \
								wifi_net    \
								button      \
								spi         \

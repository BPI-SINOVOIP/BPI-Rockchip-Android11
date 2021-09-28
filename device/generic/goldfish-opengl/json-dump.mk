JSON_DUMP :=

# Older versions of GNUmake do not support actual writing to file, so we sort of do what we can
# and write out text in chunks, escaping "
write-to-file = \
  $(eval _args:=) \
  $(foreach obj,$3,$(eval _args+=$(obj))$(if $(word $2,$(_args)),@printf "%s" $(subst ",\",$(_args)) >> $1 $(EOL)$(eval _args:=))) \
  $(if $(_args),@printf "%s" $(subst ",\", $(_args)) >> $1) \

define EOL


endef

# Functions to dump build information into a JSON tree.
# This creates a [ "", "elem1", "elem2" ]
dump-json-list = \
	$(eval JSON_DUMP += [ "" ) \
	$(if $(1),\
      $(foreach _list_item,$(strip $1),$(eval JSON_DUMP += , "$(subst ",\",$(_list_item))")) \
	) \
	$(eval JSON_DUMP += ] )\

# This creates , "name" : ["", "e1", "e2" ] 
dump-property-list = \
    $(eval JSON_DUMP += , "$(1)" : ) \
	$(call dump-json-list, $($(2)))\

# Dumps the module
dump-json-module = \
    $(eval JSON_DUMP += , { "module" : "$(_emugl_MODULE) ")\
    $(eval JSON_DUMP += ,  "path" : "$(LOCAL_PATH) ")\
    $(eval JSON_DUMP += , "type" : "$(_emugl.$(_emugl_MODULE).type)")\
	$(call dump-property-list,includes,LOCAL_C_INCLUDES) \
	$(call dump-property-list,cflags,LOCAL_CFLAGS) \
	$(call dump-property-list,libs,LOCAL_SHARED_LIBRARIES) \
	$(call dump-property-list,staticlibs,LOCAL_STATIC_LIBRARIES) \
	$(call dump-property-list,src,LOCAL_SRC_FILES) \
    $(eval JSON_DUMP += } )\

		
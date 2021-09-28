mkdir -p prebuilt-intermediates/{glsl,ir3,main,nir,spirv,cle,isl,perf,genxml,compiler,lima,midgard,iris,util,virgl,vulkan,bifrost}

python src/compiler/glsl/ir_expression_operation.py strings > prebuilt-intermediates/glsl/ir_expression_operation_strings.h 
python src/compiler/glsl/ir_expression_operation.py constant > prebuilt-intermediates/glsl/ir_expression_operation_constant.h
python src/compiler/glsl/ir_expression_operation.py enum > prebuilt-intermediates/glsl/ir_expression_operation.h

python src/freedreno/ir3/ir3_nir_trig.py -p src/compiler/nir > prebuilt-intermediates/ir3/ir3_nir_trig.c
python src/freedreno/ir3/ir3_nir_imul.py -p src/compiler/nir > prebuilt-intermediates/ir3/ir3_nir_imul.c

python src/mesa/main/format_pack.py  src/mesa/main/formats.csv  > prebuilt-intermediates/main/format_pack.c
python src/mesa/main/format_unpack.py  src/mesa/main/formats.csv  > prebuilt-intermediates/main/format_unpack.c
python src/mesa/main/format_fallback.py  src/mesa/main/formats.csv /dev/stdout  > prebuilt-intermediates/main/format_fallback.c

python src/compiler/nir/nir_builder_opcodes_h.py src/compiler/nir/nir_opcodes.py > prebuilt-intermediates/nir/nir_builder_opcodes.h
python src/compiler/nir/nir_constant_expressions.py src/compiler/nir/nir_opcodes.py > prebuilt-intermediates/nir/nir_constant_expressions.c
python src/compiler/nir/nir_opcodes_c.py src/compiler/nir/nir_opcodes.py > prebuilt-intermediates/nir/nir_opcodes.c
python src/compiler/nir/nir_opcodes_h.py src/compiler/nir/nir_opcodes.py > prebuilt-intermediates/nir/nir_opcodes.h
python src/compiler/nir/nir_opt_algebraic.py src/compiler/nir/nir_opt_algebraic.py > prebuilt-intermediates/nir/nir_opt_algebraic.c
python src/compiler/nir/nir_intrinsics_c.py --outdir prebuilt-intermediates/nir/ || ( prebuilt-intermediates/nir/nir_intrinsics.c; false)
python src/compiler/nir/nir_intrinsics_h.py --outdir prebuilt-intermediates/nir/ || ( prebuilt-intermediates/nir/nir_intrinsics.h; false)

python src/compiler/spirv/spirv_info_c.py src/compiler/spirv/spirv.core.grammar.json prebuilt-intermediates/spirv/spirv_info.c || ( prebuilt-intermediates/spirv/spirv_info.c; false)
python src/compiler/spirv/vtn_gather_types_c.py src/compiler/spirv/spirv.core.grammar.json prebuilt-intermediates/spirv/vtn_gather_types.c || ( prebuilt-intermediates/spirv/vtn_gather_types.c; false)
python src/compiler/spirv/vtn_generator_ids_h.py src/compiler/spirv/spir-v.xml prebuilt-intermediates/spirv/vtn_generator_ids.h

python src/util/format_srgb.py > prebuilt-intermediates/util/format_srgb.c

python src/intel/genxml/gen_zipped_file.py src/broadcom/cle/v3d_packet_v21.xml src/broadcom/cle/v3d_packet_v33.xml > prebuilt-intermediates/cle/v3d_xml.h

python src/broadcom/cle/gen_pack_header.py src/broadcom/cle/v3d_packet_v21.xml 21 > prebuilt-intermediates/cle/v3d_packet_v21_pack.h
python src/broadcom/cle/gen_pack_header.py src/broadcom/cle/v3d_packet_v33.xml 33 > prebuilt-intermediates/cle/v3d_packet_v33_pack.h


python src/intel/isl/gen_format_layout.py --csv src/intel/isl/isl_format_layout.csv --out prebuilt-intermediates/isl/isl_format_layout.c

python src/intel/genxml/gen_bits_header.py --cpp-guard=GENX_BITS_H  \
		src/intel/genxml/gen4.xml \
		src/intel/genxml/gen45.xml \
		src/intel/genxml/gen5.xml \
		src/intel/genxml/gen6.xml \
		src/intel/genxml/gen7.xml \
		src/intel/genxml/gen75.xml \
		src/intel/genxml/gen8.xml \
		src/intel/genxml/gen9.xml \
		src/intel/genxml/gen11.xml \
		src/intel/genxml/gen12.xml \
						> prebuilt-intermediates/genxml/genX_bits.h

python src/intel/genxml/gen_zipped_file.py \
		src/intel/genxml/gen4.xml \
		src/intel/genxml/gen45.xml \
		src/intel/genxml/gen5.xml \
		src/intel/genxml/gen6.xml \
		src/intel/genxml/gen7.xml \
		src/intel/genxml/gen75.xml \
		src/intel/genxml/gen8.xml \
		src/intel/genxml/gen9.xml \
		src/intel/genxml/gen11.xml \
		src/intel/genxml/gen12.xml \
						> prebuilt-intermediates/genxml/genX_xml.h


python  src/intel/vulkan/anv_entrypoints_gen.py --outdir prebuilt-intermediates/vulkan/ --xml src/vulkan/registry/vk.xml
python  src/intel/vulkan/anv_extensions_gen.py --xml src/vulkan/registry/vk.xml --out-c  prebuilt-intermediates/vulkan/anv_extensions.c
python  src/intel/vulkan/anv_extensions_gen.py --xml src/vulkan/registry/vk.xml --out-h  prebuilt-intermediates/vulkan/anv_extensions.h
python  src/vulkan/util/gen_enum_to_str.py  --xml src/vulkan/registry/vk.xml   --outdir prebuilt-intermediates/util/


python src/gallium/drivers/lima/ir/lima_nir_algebraic.py -p src/compiler/nir/ > prebuilt-intermediates/lima/lima_nir_algebraic.c
python src/panfrost/midgard/midgard_nir_algebraic.py -p src/compiler/nir/ > prebuilt-intermediates/midgard/midgard_nir_algebraic.c
python3 src/panfrost/bifrost/gen_disasm.py src/panfrost/bifrost/ISA.xml > prebuilt-intermediates/bifrost/bifrost_gen_disasm.c
python3 src/panfrost/bifrost/bifrost_nir_algebraic.py -p src/compiler/nir/ > prebuilt-intermediates/bifrost/bifrost_nir_algebraic.c
python3 src/panfrost/bifrost/gen_pack.py src/panfrost/bifrost/ISA.xml > prebuilt-intermediates/bifrost/bi_generated_pack.h


python src/intel/compiler/brw_nir_trig_workarounds.py -p src/compiler/nir > prebuilt-intermediates/compiler/brw_nir_trig_workarounds.c

python src/intel/perf/gen_perf.py  --code=prebuilt-intermediates/perf/gen_perf_metrics.c   --header=prebuilt-intermediates/perf/gen_perf_metrics.h \
		src/intel/perf/oa-hsw.xml \
		src/intel/perf/oa-bdw.xml \
		src/intel/perf/oa-chv.xml \
		src/intel/perf/oa-sklgt2.xml \
		src/intel/perf/oa-sklgt3.xml \
		src/intel/perf/oa-sklgt4.xml \
		src/intel/perf/oa-bxt.xml \
		src/intel/perf/oa-kblgt2.xml \
		src/intel/perf/oa-kblgt3.xml \
		src/intel/perf/oa-glk.xml \
		src/intel/perf/oa-cflgt2.xml \
		src/intel/perf/oa-cflgt3.xml \
		src/intel/perf/oa-icl.xml \
		src/intel/perf/oa-lkf.xml \
		src/intel/perf/oa-tgl.xml

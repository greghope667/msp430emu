set_languages("gnuxx20")
add_cxflags("-march=native")
add_cxflags("-Wall", "-Wextra")
add_cxflags("-Og", "-g")

target("msp430emu-cli")
	set_kind("binary")
	add_files("src/*.cpp")

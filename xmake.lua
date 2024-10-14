set_languages("gnu17", "gnuxx20")
add_cxflags("-march=native")
add_cxflags("-Wall", "-Wextra")
add_cxflags("-Og", "-g")

target("termbox2")
	set_kind("object")
	add_cxflags("-O2", "-Wno-unused-result")
	add_includedirs("lib/termbox2", {public = true})
	add_files("lib/termbox2.c")

target("msp430emu-cli")
	set_kind("binary")
	add_files("src/main_cli.cpp", "src/msp430.cpp")

target("msp430emu-tui")
	set_kind("binary")
	add_files("src/main_tui.cpp", "src/msp430.cpp")
	add_deps("termbox2")

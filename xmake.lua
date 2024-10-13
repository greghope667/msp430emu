add_requires("notcurses-core", { system = true })
set_languages("gnuxx20")
add_cxflags("-march=native")
add_cxflags("-Wall", "-Wextra")
add_cxflags("-Og", "-g")

target("msp430emu-cli")
	set_kind("binary")
	add_files("src/main_cli.cpp", "src/msp430.cpp")

target("msp430emu-tui")
	set_kind("binary")
	add_files("src/main_tui.cpp", "src/msp430.cpp")
	add_packages("notcurses-core")

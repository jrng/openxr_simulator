#define C_MAKE_IMPLEMENTATION
#include "src/c_make.h"

C_MAKE_INFO(commands_info, configs_info)
{
    add_default_info(commands_info, configs_info);
}

C_MAKE_ENTRY(command, argument_count, arguments)
{
    if (strings_are_equal(command, COMMAND_SETUP))
    {
        config_set_if_not_exists("trace_openxr_calls", "off");

        switch (get_target_platform())
        {
            case PlatformWindows:
            {
                config_set_if_not_exists("platform_win32", "on");

                config_set_if_not_exists("graphics_api_d3d11", "on");
                config_set_if_not_exists("graphics_api_opengl", "on");
            } break;

            case PlatformLinux:
            {
                config_set_if_not_exists("platform_xlib", "on");
                config_set_if_not_exists("platform_wayland", "on");

                config_set_if_not_exists("graphics_api_opengl", "on");
            } break;

            default: break;
        }
    }
    else if (strings_are_equal(command, COMMAND_BUILD))
    {
        bool trace_openxr_calls = config_is_enabled("trace_openxr_calls", false);

        bool platform_xlib = config_is_enabled("platform_xlib", false);
        bool platform_win32 = config_is_enabled("platform_win32", false);
        bool platform_wayland = config_is_enabled("platform_wayland", false);

        bool graphics_api_d3d11 = config_is_enabled("graphics_api_d3d11", false);
        bool graphics_api_opengl = config_is_enabled("graphics_api_opengl", false);

        Command cmd = { 0 };

        command_append(&cmd, get_target_c_compiler());
        command_append_command_line(&cmd, get_target_c_flags());
        command_append_default_compiler_flags(&cmd, get_build_type());

        if (compiler_is_msvc(get_target_c_compiler()))
        {
            command_append(&cmd, "-LD");
        }
        else
        {
            // command_append(&cmd, "-std=c99", "-Wall", "-Wextra", "-pedantic");
            command_append(&cmd, "-std=c99");
            command_append(&cmd, "-fPIC", "-shared");
        }

        ConfigValue openxr_sdk_root_path = config_get("openxr_sdk_root_path");

        if (openxr_sdk_root_path.is_valid &&
            (string_trim(CString(openxr_sdk_root_path.val)).count > 0))
        {
            command_append(&cmd, c_string_concat("-I", c_string_path_concat(openxr_sdk_root_path.val, "include")));
        }

        if (trace_openxr_calls)
        {
            command_append(&cmd, "-DTRACE_OPENXR_CALLS=1");
        }

        if (platform_xlib)
        {
            command_append(&cmd, "-DPLATFORM_XLIB=1");
        }

        if (platform_win32)
        {
            command_append(&cmd, "-DPLATFORM_WIN32=1");
        }

        if (platform_wayland)
        {
            command_append(&cmd, "-DPLATFORM_WAYLAND=1");
        }

        if (graphics_api_d3d11)
        {
            command_append(&cmd, "-DGRAPHICS_API_D3D11=1");
        }

        if (graphics_api_opengl)
        {
            command_append(&cmd, "-DGRAPHICS_API_OPENGL=1");
        }

        command_append_output_shared_library(&cmd, c_string_path_concat(get_build_path(), "openxr_simulator"), get_target_platform());
        command_append(&cmd, c_string_path_concat(get_source_path(), "src", "runtime.c"));
        command_append_default_linker_flags(&cmd, get_target_architecture());

        if (get_target_platform() == PlatformWindows)
        {
            if (compiler_is_msvc(get_target_c_compiler()))
            {
                command_append(&cmd, "user32.lib", "gdi32.lib");

                if (graphics_api_d3d11)
                {
                    command_append(&cmd, "dxgi.lib", "dxguid.lib", "d3dcompiler.lib");
                }

                if (graphics_api_opengl)
                {
                    command_append(&cmd, "opengl32.lib");
                }
            }
            else
            {
                command_append(&cmd, "-luser32", "-lgdi32");

                if (graphics_api_d3d11)
                {
                    command_append(&cmd, "-ldxgi", "-ldxguid", "-ld3dcompiler");
                }

                if (graphics_api_opengl)
                {
                    command_append(&cmd, "-lopengl32");
                }
            }
        }
        else
        {
            if (get_host_platform() != PlatformMacOs)
            {
                command_append(&cmd, "-Wl,--no-undefined", "-lm");
            }

            if (graphics_api_opengl)
            {
                command_append(&cmd, "-lGL");
            }
        }

        if (platform_xlib)
        {
            command_append(&cmd, "-lX11");
        }

        if (platform_wayland)
        {
            command_append(&cmd, "-lwayland-client");
        }

        c_make_log(LogLevelInfo, "compile 'openxr_simulator'\n");
        command_run(cmd);
    }
    else
    {
        handle_default_commands(command, argument_count, arguments);
    }
}

// Based on:
// https://github.com/jvm-profiling-tools/perf-map-agent/blob/master/src/c/perf-map-agent.c

/*
 *   libperfmap: a JVM agent to create perf-<pid>.map files for consumption
 *               with linux perf-tools
 *   Copyright (C) 2013-2015 Johannes Rudolph<johannes.rudolph@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <lo2s/ipc/fifo.hpp>

#include <iomanip>
#include <iostream>
#include <memory>

#include <jni.h>
#include <jvmti.h>
#include <jvmticmlr.h>

extern "C"
{
#include <string.h>
#include <unistd.h>
}

static std::unique_ptr<lo2s::ipc::Fifo> fifo;

static std::fstream f("/tmp/mario-java-test.cpp");

static void JNICALL cbCompiledMethodLoad(jvmtiEnv* jvmti, jmethodID method, jint len,
                                         const void* address, jint map_length,
                                         const jvmtiAddrLocationMap* map, const void* compile_info)
{
    std::cerr << "JNI lo2s plugin: cbCompiledMethodLoad" << std::endl;

    (void)jvmti;
    (void)method;
    (void)len;
    (void)address;
    (void)map_length;
    (void)map;
    (void)compile_info;

    if (!fifo)
    {
        return;
    }

    char* name_ptr;
    char* signature_ptr;
    char* generic_ptr;

    jvmti->GetMethodName(jvmti, method, &name_ptr, &signature_ptr, &generic_ptr);

    std::cerr << std::hex << reinterpret_cast<std::uint64_t>(address) << " " << *len << " "
              << *name_str;
    if (signature_ptr)
    {
        std::cerr << " | " << *signature_ptr;

        fifo->write(reinterpret_cast<std::uint64_t>(address));
        fifo->write(len);
        fifo->write(name_str);

        jvmti->Deallocate((unsigned char*)signature_ptr);
    }

    if (generic_ptr)
    {
        std::cerr << " " << *generic_ptr;
        jvmti->Deallocate((unsigned char*)generic_ptr);
    }

    std::cerr << std::endl;

    jvmti->Deallocate((unsigned char*)name_ptr);
}

void JNICALL cbDynamicCodeGenerated(jvmtiEnv* jvmti, const char* name, const void* address,
                                    jint length)
{
    std::cerr << "JNI lo2s plugin: cbDynamicCodeGenerated" << std::endl;

    (void)jvmti;

    if (!fifo)
    {
        return;
    }

    int len = length;
    std::string name_str = name;

    try
    {
        fifo->write(reinterpret_cast<std::uint64_t>(address));
        fifo->write(len);
        fifo->write(name_str);
        std::cerr << std::hex << reinterpret_cast<std::uint64_t>(address) << " " << len << " "
                  << name_str << std::endl;
    }
    catch (...)
    {
    }
}

jvmtiError enable_capabilities(jvmtiEnv* jvmti)
{
    std::cerr << "Loading JNI lo2s plugin: enable capabilities" << std::endl;

    jvmtiCapabilities capabilities;

    memset(&capabilities, 0, sizeof(capabilities));
    capabilities.can_generate_all_class_hook_events = 1;
    capabilities.can_tag_objects = 1;
    capabilities.can_generate_object_free_events = 1;
    capabilities.can_get_source_file_name = 1;
    capabilities.can_get_line_numbers = 1;
    capabilities.can_generate_vm_object_alloc_events = 1;
    capabilities.can_generate_compiled_method_load_events = 1;

    // Request these capabilities for this JVM TI environment.
    return jvmti->AddCapabilities(&capabilities);
}

jvmtiError set_callbacks(jvmtiEnv* jvmti)
{
    std::cerr << "Loading JNI lo2s plugin: set callbacks" << std::endl;

    jvmtiEventCallbacks callbacks;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.CompiledMethodLoad = &cbCompiledMethodLoad;
    callbacks.DynamicCodeGenerated = &cbDynamicCodeGenerated;
    return jvmti->SetEventCallbacks(&callbacks, (jint)sizeof(callbacks));
}

void set_notification_mode(jvmtiEnv* jvmti, jvmtiEventMode mode)
{
    std::cerr << "Loading JNI lo2s plugin: set notification mode" << std::endl;

    jvmti->SetEventNotificationMode(mode, JVMTI_EVENT_COMPILED_METHOD_LOAD, (jthread)NULL);
    jvmti->SetEventNotificationMode(mode, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, (jthread)NULL);
}

extern "C"
{
    JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm, char* options, void* reserved)
    {
        std::cerr << "Loading JNI lo2s plugin" << std::endl;
        try
        {
            fifo = std::make_unique<lo2s::ipc::Fifo>(getpid(), "jvmti");
        }
        catch (...)
        {
            return 0;
        }

        jvmtiEnv* jvmti;
        vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1);
        enable_capabilities(jvmti);
        set_callbacks(jvmti);
        set_notification_mode(jvmti, JVMTI_ENABLE);
        jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
        jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
        set_notification_mode(jvmti, JVMTI_DISABLE);

        return 0;
    }

    JNIEXPORT void JNICALL Agent_OnUnload(JavaVM*)
    {
        std::cerr << "Unloading JNI lo2s plugin" << std::endl;

        fifo.reset(nullptr);
    }
}

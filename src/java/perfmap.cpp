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
#include <lo2s/java/jni_signature.hpp>
#include <lo2s/log.hpp>

#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include <jni.h>
#include <jvmti.h>
#include <jvmticmlr.h>

extern "C"
{
#include <string.h>
#include <unistd.h>
}

using lo2s::Log;

static std::unique_ptr<lo2s::ipc::Fifo> fifo;
static std::mutex mutex_;

static void JNICALL cbCompiledMethodLoad(jvmtiEnv* jvmti, jmethodID method, jint code_size,
                                         const void* address, jint map_length,
                                         const jvmtiAddrLocationMap* map, const void* compile_info)
{
    (void)map_length;
    (void)map;
    (void)compile_info;

    if (!fifo)
    {
        return;
    }

    std::string symbol_name;

    jclass cls;

    jvmti->GetMethodDeclaringClass(method, &cls);

    {
        char* class_signature_ptr;
        jvmti->GetClassSignature(cls, &class_signature_ptr, nullptr);

        if (class_signature_ptr)
        {
            symbol_name = lo2s::java::parse_type(class_signature_ptr) + "::";
            jvmti->Deallocate((unsigned char*)class_signature_ptr);
        }
    }

    char* name_ptr;
    char* signature_ptr;

    jvmti->GetMethodName(method, &name_ptr, &signature_ptr, nullptr);

    if (name_ptr)
    {
        symbol_name += name_ptr;
        jvmti->Deallocate((unsigned char*)name_ptr);
    }

    if (signature_ptr)
    {
        symbol_name += lo2s::java::parse_signature(signature_ptr);
        jvmti->Deallocate((unsigned char*)signature_ptr);
    }

    Log::debug() << "cbCompiledMethodLoad: 0x" << std::hex
                 << reinterpret_cast<std::uint64_t>(address) << " " << std::dec << code_size << ": "
                 << symbol_name;

    {
        auto lg = Log::debug() << "cbCompiledMethodLoad map of size " << map_length << ":\n";

        for (int i = 0; i < map_length - 1; i++)
        {
            lg << "0x" << std::hex << reinterpret_cast<std::uint64_t>(map[i].start_address)
               << " - 0x" << reinterpret_cast<std::uint64_t>(map[i + 1].start_address) - 1
               << " ====> location " << std::dec << map[i].location << "\n";
        }
    }

    if (!symbol_name.empty() && map_length >= 2)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // TODO Assumption: the method is contiguous in memory.

        int len = reinterpret_cast<std::uint64_t>(map[map_length - 1].start_address) - 1 -
                  reinterpret_cast<std::uint64_t>(map[0].start_address);

        fifo->write(reinterpret_cast<std::uint64_t>(map[0].start_address));
        fifo->write(len);
        fifo->write(symbol_name);
    }
}

void JNICALL cbDynamicCodeGenerated(jvmtiEnv* jvmti, const char* name, const void* address,
                                    jint length)
{
    (void)jvmti;

    if (!fifo)
    {
        return;
    }

    int len = length;
    std::string name_str = name;

    try
    {
        std::lock_guard<std::mutex> lock(mutex_);

        fifo->write(reinterpret_cast<std::uint64_t>(address));
        fifo->write(len);
        fifo->write(name_str);
        Log::debug() << "cbDynamicCodeGenerated: 0x" << std::hex
                     << reinterpret_cast<std::uint64_t>(address) << " " << len << ": " << name_str;
    }
    catch (...)
    {
    }
}

static void JNICALL cbClassPrepare(jvmtiEnv* jvmti, JNIEnv*, jthread, jclass cls)
{
    std::string class_str;

    {
        char* class_signature_ptr;
        jvmti->GetClassSignature(cls, &class_signature_ptr, nullptr);
        class_str = lo2s::java::parse_type(class_signature_ptr);
        jvmti->Deallocate((unsigned char*)class_signature_ptr);
    }

    Log::debug() << "class loaded: " << class_str;

    jint method_count;
    jmethodID* methods_ptr;

    jvmti->GetClassMethods(cls, &method_count, &methods_ptr);

    for (jint i = 0; i < method_count; ++i)
    {
        char* name_ptr;
        char* signature_ptr;

        jvmti->GetMethodName(methods_ptr[i], &name_ptr, &signature_ptr, nullptr);

        std::string name_str(class_str);
        name_str += std::string("::") + name_ptr + lo2s::java::parse_signature(signature_ptr);

        std::uint64_t address;

        jlocation begin;
        jlocation end;

        jvmti->GetMethodLocation(methods_ptr[i], &begin, &end);
        int len = end - begin;

        if (begin == 0)
        {
            // only got bytecode offsets, so use them instead
            jint count;
            unsigned char* bytecodes;

            jvmti->GetBytecodes(methods_ptr[i], &count, &bytecodes);

            address = reinterpret_cast<std::uint64_t>(bytecodes);
        }
        else
        {
            address = static_cast<std::uint64_t>(begin);
        }

        if (len == 0)
        {
            continue;
        }

        try
        {
            std::lock_guard<std::mutex> lock(mutex_);

            fifo->write(address);
            fifo->write(len);
            fifo->write(name_str);
            Log::debug() << "cbClassPrepare: 0x" << std::hex << address << " " << len << ": "
                         << name_str;
        }
        catch (...)
        {
        }
    }

    Log::trace() << "class load finished: " << class_str;
}

jvmtiError enable_capabilities(jvmtiEnv* jvmti)
{
    jvmtiCapabilities capabilities;

    memset(&capabilities, 0, sizeof(capabilities));
    capabilities.can_generate_all_class_hook_events = 1;
    capabilities.can_tag_objects = 1;
    capabilities.can_generate_object_free_events = 1;
    capabilities.can_get_source_file_name = 1;
    capabilities.can_get_line_numbers = 1;
    capabilities.can_generate_vm_object_alloc_events = 1;
    capabilities.can_generate_compiled_method_load_events = 1;
    capabilities.can_get_bytecodes = 1;

    // Request these capabilities for this JVM TI environment.
    return jvmti->AddCapabilities(&capabilities);
}

jvmtiError set_callbacks(jvmtiEnv* jvmti)
{
    jvmtiEventCallbacks callbacks;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.CompiledMethodLoad = &cbCompiledMethodLoad;
    callbacks.DynamicCodeGenerated = &cbDynamicCodeGenerated;
    // perfj doesn't use this either
    // so it's fine I guess
    // callbacks.ClassPrepare = &cbClassPrepare;
    (void)&cbClassPrepare;
    return jvmti->SetEventCallbacks(&callbacks, (jint)sizeof(callbacks));
}

void set_notification_mode(jvmtiEnv* jvmti, jvmtiEventMode mode)
{
    jvmti->SetEventNotificationMode(mode, JVMTI_EVENT_COMPILED_METHOD_LOAD, (jthread)NULL);
    jvmti->SetEventNotificationMode(mode, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, (jthread)NULL);
    jvmti->SetEventNotificationMode(mode, JVMTI_EVENT_CLASS_PREPARE, (jthread)NULL);
}

extern "C"
{
    JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* vm, char*, void*)
    {
        Log::info() << "Attaching JNI lo2s plugin";
        lo2s::logging::set_min_severity_level(nitro::log::severity_level::info);
        try
        {
            fifo = std::make_unique<lo2s::ipc::Fifo>(lo2s::Process{ getpid() }, "jvmti");
        }
        catch (...)
        {
            Log::fatal() << "Failed to open IPC pipe to lo2s in the JNI plugin. Won't be able to "
                            "resolve JVM methods.";

            return 0;
        }

        jvmtiEnv* jvmti;
        vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1);
        enable_capabilities(jvmti);

        // check jLocation format
        jvmtiJlocationFormat format;
        jvmti->GetJLocationFormat(&format);
        if (format != 1)
        {
            Log::warn() << "Unsupported jLocation Format: " << format;
        }

        set_callbacks(jvmti);
        set_notification_mode(jvmti, JVMTI_ENABLE);
        jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
        jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
        // set_notification_mode(jvmti, JVMTI_DISABLE);

        return 0;
    }

    JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char*, void*)
    {
        Log::info() << "Loading JNI lo2s plugin";
        lo2s::logging::set_min_severity_level(nitro::log::severity_level::info);
        try
        {
            fifo = std::make_unique<lo2s::ipc::Fifo>(lo2s::Process{ getpid() }, "jvmti");
        }
        catch (...)
        {
            Log::fatal() << "Failed to open IPC pipe to lo2s in the JNI plugin. Won't be able to "
                            "resolve JVM methods.";

            return 0;
        }

        jvmtiEnv* jvmti;
        vm->GetEnv((void**)&jvmti, JVMTI_VERSION_1);
        enable_capabilities(jvmti);

        // check jLocation format
        jvmtiJlocationFormat format;
        jvmti->GetJLocationFormat(&format);
        if (format != 1)
        {
            Log::warn() << "Unsupported jLocation Format: " << format;
        }

        set_callbacks(jvmti);
        set_notification_mode(jvmti, JVMTI_ENABLE);
        jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
        jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
        // set_notification_mode(jvmti, JVMTI_DISABLE);

        return 0;
    }

    JNIEXPORT void JNICALL Agent_OnUnload(JavaVM*)
    {
        Log::info() << "Unloading JNI lo2s plugin";

        fifo.reset(nullptr);
    }
}

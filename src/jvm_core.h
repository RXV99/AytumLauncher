#ifndef JVM_CORE_H
#define JVM_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * JVM Core Types
 * ============================================================ */

typedef uint8_t  u1;
typedef uint16_t u2;
typedef uint32_t u4;
typedef uint64_t u8;
typedef int8_t   s1;
typedef int16_t  s2;
typedef int32_t  s4;
typedef int64_t  s8;

/* Constant pool tags */
enum {
    CONSTANT_Utf8               = 1,
    CONSTANT_Integer            = 3,
    CONSTANT_Float              = 4,
    CONSTANT_Long               = 5,
    CONSTANT_Double             = 6,
    CONSTANT_Class              = 7,
    CONSTANT_String             = 8,
    CONSTANT_Fieldref           = 9,
    CONSTANT_Methodref          = 10,
    CONSTANT_InterfaceMethodref = 11,
    CONSTANT_NameAndType        = 12,
    CONSTANT_MethodHandle       = 15,
    CONSTANT_MethodType         = 16,
    CONSTANT_InvokeDynamic      = 18,
};

/* Field access flags */
enum {
    ACC_PUBLIC       = 0x0001,
    ACC_PRIVATE      = 0x0002,
    ACC_PROTECTED    = 0x0004,
    ACC_STATIC       = 0x0008,
    ACC_FINAL        = 0x0010,
    ACC_VOLATILE     = 0x0040,
    ACC_TRANSIENT    = 0x0080,
};

/* Method access flags */
enum {
    ACC_SYNCHRONIZED = 0x0020,
    ACC_BRIDGE       = 0x0040,
    ACC_VARARGS      = 0x0080,
    ACC_NATIVE       = 0x0100,
    ACC_ABSTRACT     = 0x0400,
    ACC_STRICT       = 0x0800,
};

/* Max JVM heap size */
#define JVM_HEAP_SIZE (8 * 1024 * 1024)
#define JVM_STACK_SIZE (64 * 1024)
#define JVM_MAX_THREADS 8

/* Value type union */
typedef union {
    s4   i;
    s8   l;
    float f;
    double d;
    void *ref;
} jvm_value;

/* Object header */
typedef struct jvm_object {
    struct jvm_class *class_info;
    uint32_t monitor;
    uint32_t flags;
    size_t data_size;
    uint8_t data[];
} jvm_object;

/* Array object */
typedef struct jvm_array {
    jvm_object header;
    int32_t length;
    uint8_t elements[];
} jvm_array;

/* Constant pool entry */
typedef struct {
    u1 tag;
    union {
        struct { u2 string_index; } utf8;
        struct { u2 class_index; } class_ref;
        struct { u2 string_index; } string_ref;
        struct { u2 class_index; u2 nat_index; } member_ref;
        struct { u2 name_index; u2 desc_index; } name_and_type;
        s4 integer;
        float float_val;
        s8 long_val;
        double double_val;
        struct { u2 ref_kind; u2 ref_index; } method_handle;
        struct { u2 desc_index; } method_type;
        struct { u2 bootstrap_method_attr_index; u2 name_index; u2 desc_index; } invoke_dynamic;
    } data;
} cp_info;

/* Field info */
typedef struct {
    u2 access_flags;
    u2 name_index;
    u2 descriptor_index;
    u2 attributes_count;
    u2 const_value_index;
} field_info;

/* Method info */
typedef struct {
    u2 access_flags;
    u2 name_index;
    u2 descriptor_index;
    u2 attributes_count;
    /* Code attribute */
    u2 max_stack;
    u2 max_locals;
    u4 code_length;
    u1 *code;
    u2 exception_table_length;
    struct {
        u2 start_pc;
        u2 end_pc;
        u2 handler_pc;
        u2 catch_type;
    } *exception_table;
} method_info;

/* Class info */
typedef struct jvm_class {
    struct jvm_class *next;
    u2 access_flags;
    u2 this_class;
    u2 super_class;
    u2 interfaces_count;
    u2 *interfaces;
    u2 fields_count;
    field_info *fields;
    u2 methods_count;
    method_info *methods;
    u2 cp_count;
    cp_info *cp;
    char *source_file;
    jvm_object *static_data;
} jvm_class;

/* Frame */
typedef struct jvm_frame {
    method_info *method;
    jvm_class *class_info;
    u1 *pc;
    jvm_value *locals;
    jvm_value *stack;
    s4 stack_top;
    u2 max_stack;
    u2 max_locals;
    struct jvm_frame *prev;
} jvm_frame;

/* Thread state */
typedef struct {
    uint32_t id;
    char name[64];
    jvm_frame *frames;
    uint32_t stack_used;
    uint8_t stack_data[JVM_STACK_SIZE];
    int running;
} jvm_thread;

/* JVM instance */
typedef struct {
    jvm_class *classes;
    jvm_class *primitive_classes[9];
    int class_count;
    uint8_t heap[JVM_HEAP_SIZE];
    size_t heap_used;
    jvm_thread threads[JVM_MAX_THREADS];
    int thread_count;
    int current_thread;
} jvm_instance;

/* ============================================================
 * API Functions
 * ============================================================ */

jvm_instance *jvm_create(void);
void jvm_destroy(jvm_instance *jvm);

jvm_class *jvm_load_class(jvm_instance *jvm, const uint8_t *data, size_t size);
jvm_class *jvm_find_class(jvm_instance *jvm, const char *name);

jvm_object *jvm_alloc_object(jvm_instance *jvm, jvm_class *class_info);
jvm_array  *jvm_alloc_array(jvm_instance *jvm, jvm_class *class_info, int32_t length);

jvm_value  jvm_execute_method(jvm_instance *jvm, jvm_class *class_info, method_info *method,
                              jvm_value *args, int argc);

/* Native method registration */
typedef void (*native_method_fn)(jvm_instance *jvm, jvm_thread *thread);

void jvm_register_native(jvm_instance *jvm, const char *class_name,
                         const char *method_name, const char *descriptor,
                         native_method_fn fn);

void jvm_run(jvm_instance *jvm, const char *midlet_class);

/* Stack operations */
jvm_value jvm_stack_pop(jvm_thread *thread);
void      jvm_stack_push(jvm_thread *thread, jvm_value val);
jvm_value jvm_local_get(jvm_thread *thread, u2 index);
void      jvm_local_set(jvm_thread *thread, u2 index, jvm_value val);

#ifdef __cplusplus
}
#endif

#endif /* JVM_CORE_H */

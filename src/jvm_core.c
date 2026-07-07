#include "jvm_core.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>

/* ============================================================
 * Internal helpers
 * ============================================================ */

static inline u2 read_u2(const u1 **p) { u2 v = ((u2)(*p)[0] << 8) | (*p)[1]; *p += 2; return v; }
static inline u4 read_u4(const u1 **p) { u4 v = ((u4)(*p)[0] << 24) | ((u4)(*p)[1] << 16) | ((u4)(*p)[2] << 8) | (*p)[3]; *p += 4; return v; }

static char *cp_utf8(jvm_class *c, u2 idx) {
    if (idx <= 0 || idx >= c->cp_count) return NULL;
    cp_info *cp = &c->cp[idx];
    if (cp->tag != CONSTANT_Utf8) return NULL;
    return (char*)&c->cp[cp->data.utf8.string_index];
}

static jvm_value make_int(s4 v) { jvm_value val; val.i = v; return val; }
static jvm_value make_ref(void *r) { jvm_value val; val.ref = r; return val; }

/* ============================================================
 * Class file loader
 * ============================================================ */

jvm_class *jvm_load_class(jvm_instance *jvm, const uint8_t *data, size_t size) {
    const u1 *p = data;
    u4 magic = read_u4(&p);
    if (magic != 0xCAFEBABE) {
        printf("jvm: bad magic 0x%08X\n", magic);
        return NULL;
    }

    u2 minor_ver = read_u2(&p);
    u2 major_ver = read_u2(&p);
    (void)minor_ver; (void)major_ver;

    jvm_class *cls = (jvm_class*)calloc(1, sizeof(jvm_class));
    if (!cls) return NULL;

    /* Constant pool */
    cls->cp_count = read_u2(&p);
    cls->cp = (cp_info*)calloc(cls->cp_count, sizeof(cp_info));
    if (!cls->cp) { free(cls); return NULL; }

    for (u2 i = 1; i < cls->cp_count; i++) {
        cp_info *cp = &cls->cp[i];
        cp->tag = *p++;
        switch (cp->tag) {
            case CONSTANT_Utf8: {
                u2 len = read_u2(&p);
                char *str = (char*)calloc(len + 1, 1);
                memcpy(str, p, len);
                p += len;
                /* Store the string as a hack: we use cp->data.utf8.string_index to store offset into the cp block itself.
                   Instead, let's store it in the NameAndType's name_index field. Actually, let's use string_index field. */
                cp->data.utf8.string_index = (u2)(uintptr_t)str;
                break;
            }
            case CONSTANT_Integer:
                cp->data.integer = (s4)read_u4(&p);
                break;
            case CONSTANT_Float:
                { uint32_t tmp_f = read_u4(&p);
                  memcpy(&cp->data.float_val, &tmp_f, sizeof(float)); }
                break;
            case CONSTANT_Long:
                cp->data.long_val = ((s8)read_u4(&p) << 32) | read_u4(&p);
                i++;
                break;
            case CONSTANT_Double:
                { uint32_t hi = read_u4(&p), lo = read_u4(&p);
                  uint64_t tmp = ((uint64_t)hi << 32) | lo;
                  memcpy(&cp->data.double_val, &tmp, sizeof(double)); i++; break; }
            case CONSTANT_Class:
                cp->data.class_ref.class_index = read_u2(&p);
                break;
            case CONSTANT_String:
                cp->data.string_ref.string_index = read_u2(&p);
                break;
            case CONSTANT_Fieldref:
            case CONSTANT_Methodref:
            case CONSTANT_InterfaceMethodref:
                cp->data.member_ref.class_index = read_u2(&p);
                cp->data.member_ref.nat_index = read_u2(&p);
                break;
            case CONSTANT_NameAndType:
                cp->data.name_and_type.name_index = read_u2(&p);
                cp->data.name_and_type.desc_index = read_u2(&p);
                break;
            case CONSTANT_MethodHandle:
                cp->data.method_handle.ref_kind = *p++;
                cp->data.method_handle.ref_index = read_u2(&p);
                break;
            case CONSTANT_MethodType:
                cp->data.method_type.desc_index = read_u2(&p);
                break;
            case CONSTANT_InvokeDynamic:
                cp->data.invoke_dynamic.bootstrap_method_attr_index = read_u2(&p);
                cp->data.invoke_dynamic.name_index = read_u2(&p);
                cp->data.invoke_dynamic.desc_index = read_u2(&p);
                break;
            default:
                printf("jvm: unknown cp tag %d at %d\n", cp->tag, i);
                break;
        }
    }

    cls->access_flags = read_u2(&p);
    cls->this_class = read_u2(&p);
    cls->super_class = read_u2(&p);
    cls->interfaces_count = read_u2(&p);
    cls->interfaces = (u2*)calloc(cls->interfaces_count, sizeof(u2));
    for (u2 i = 0; i < cls->interfaces_count; i++)
        cls->interfaces[i] = read_u2(&p);

    cls->fields_count = read_u2(&p);
    cls->fields = (field_info*)calloc(cls->fields_count, sizeof(field_info));
    for (u2 i = 0; i < cls->fields_count; i++) {
        field_info *f = &cls->fields[i];
        f->access_flags = read_u2(&p);
        f->name_index = read_u2(&p);
        f->descriptor_index = read_u2(&p);
        f->attributes_count = read_u2(&p);
        for (u2 j = 0; j < f->attributes_count; j++) {
            read_u2(&p); /* attr name */
            u4 attr_len = read_u4(&p);
            if (attr_len >= 2) {
                f->const_value_index = read_u2(&p);
                p += attr_len - 2;
            } else {
                p += attr_len;
            }
        }
    }

    cls->methods_count = read_u2(&p);
    cls->methods = (method_info*)calloc(cls->methods_count, sizeof(method_info));
    for (u2 i = 0; i < cls->methods_count; i++) {
        method_info *m = &cls->methods[i];
        m->access_flags = read_u2(&p);
        m->name_index = read_u2(&p);
        m->descriptor_index = read_u2(&p);
        m->attributes_count = read_u2(&p);
        m->code = NULL;
        for (u2 j = 0; j < m->attributes_count; j++) {
            u2 attr_name_idx = read_u2(&p);
            u4 attr_len = read_u4(&p);
            const u1 *attr_end = p + attr_len;
            char *attr_name = cp_utf8(cls, attr_name_idx);
            if (attr_name && strcmp(attr_name, "Code") == 0) {
                m->max_stack = read_u2(&p);
                m->max_locals = read_u2(&p);
                m->code_length = read_u4(&p);
                m->code = (u1*)calloc(m->code_length, 1);
                if (m->code) memcpy(m->code, p, m->code_length);
                p += m->code_length;
                m->exception_table_length = read_u2(&p);
                m->exception_table = NULL;
                if (m->exception_table_length > 0) {
                    m->exception_table = calloc(m->exception_table_length, sizeof(*m->exception_table));
                    for (u2 k = 0; k < m->exception_table_length; k++) {
                        m->exception_table[k].start_pc = read_u2(&p);
                        m->exception_table[k].end_pc = read_u2(&p);
                        m->exception_table[k].handler_pc = read_u2(&p);
                        m->exception_table[k].catch_type = read_u2(&p);
                    }
                }
                /* Skip remaining code attributes */
                u2 code_attr_count = read_u2(&p);
                for (u2 k = 0; k < code_attr_count; k++) {
                    read_u2(&p);
                    u4 ca_len = read_u4(&p);
                    p += ca_len;
                }
            } else if (attr_name && strcmp(attr_name, "Exceptions") == 0) {
                u2 exc_count = read_u2(&p);
                p += exc_count * 2;
            } else {
                p = attr_end;
            }
        }
    }

    /* Attributes */
    u2 attr_count = read_u2(&p);
    for (u2 i = 0; i < attr_count; i++) {
        u2 name_idx = read_u2(&p);
        u4 attr_len = read_u4(&p);
        char *name = cp_utf8(cls, name_idx);
        if (name && strcmp(name, "SourceFile") == 0) {
            u2 sf_idx = read_u2(&p);
            cls->source_file = cp_utf8(cls, sf_idx);
        } else {
            p += attr_len;
        }
    }

    /* Add to class list */
    cls->next = jvm->classes;
    jvm->classes = cls;
    jvm->class_count++;

    /* Allocate static data */
    size_t static_size = cls->fields_count * sizeof(jvm_value);
    if (static_size > 0) {
        cls->static_data = (jvm_object*)calloc(1, sizeof(jvm_object) + static_size);
        if (cls->static_data) cls->static_data->data_size = static_size;
    }

    return cls;
}

jvm_class *jvm_find_class(jvm_instance *jvm, const char *name) {
    for (jvm_class *c = jvm->classes; c; c = c->next) {
        char *cls_name = cp_utf8(c, c->this_class);
        if (cls_name && strcmp(cls_name, name) == 0)
            return c;
    }
    return NULL;
}

/* ============================================================
 * Memory allocation
 * ============================================================ */

jvm_object *jvm_alloc_object(jvm_instance *jvm, jvm_class *class_info) {
    size_t size = sizeof(jvm_object);
    if (jvm->heap_used + size > JVM_HEAP_SIZE) return NULL;

    jvm_object *obj = (jvm_object*)(jvm->heap + jvm->heap_used);
    jvm->heap_used += size;
    memset(obj, 0, size);
    obj->class_info = class_info;
    obj->data_size = 0;
    return obj;
}

jvm_array *jvm_alloc_array(jvm_instance *jvm, jvm_class *class_info, int32_t length) {
    size_t size = sizeof(jvm_array) + (size_t)length * sizeof(jvm_value);
    if (jvm->heap_used + size > JVM_HEAP_SIZE) return NULL;

    jvm_array *arr = (jvm_array*)(jvm->heap + jvm->heap_used);
    jvm->heap_used += size;
    memset(arr, 0, size);
    arr->header.class_info = class_info;
    arr->length = length;
    return arr;
}

/* ============================================================
 * Stack operations
 * ============================================================ */

jvm_value jvm_stack_pop(jvm_thread *thread) {
    jvm_frame *f = thread->frames;
    if (f->stack_top < 0) {
        printf("jvm: stack underflow!\n");
        jvm_value v; memset(&v, 0, sizeof(v)); return v;
    }
    return f->stack[f->stack_top--];
}

void jvm_stack_push(jvm_thread *thread, jvm_value val) {
    jvm_frame *f = thread->frames;
    if (f->stack_top >= (s4)f->max_stack - 1) {
        printf("jvm: stack overflow!\n");
        return;
    }
    f->stack[++f->stack_top] = val;
}

jvm_value jvm_local_get(jvm_thread *thread, u2 index) {
    jvm_frame *f = thread->frames;
    if (index >= f->max_locals) {
        printf("jvm: local index out of bounds %d >= %d\n", index, f->max_locals);
        jvm_value v; memset(&v, 0, sizeof(v)); return v;
    }
    return f->locals[index];
}

void jvm_local_set(jvm_thread *thread, u2 index, jvm_value val) {
    jvm_frame *f = thread->frames;
    if (index >= f->max_locals) return;
    f->locals[index] = val;
}

/* ============================================================
 * Native method table
 * ============================================================ */

#define MAX_NATIVES 256

typedef struct {
    char class_name[128];
    char method_name[128];
    char descriptor[128];
    native_method_fn fn;
} native_entry;

static native_entry native_table[MAX_NATIVES];
static int native_count = 0;

void jvm_register_native(jvm_instance *jvm, const char *class_name,
                         const char *method_name, const char *descriptor,
                         native_method_fn fn) {
    (void)jvm;
    if (native_count >= MAX_NATIVES) return;
    strncpy(native_table[native_count].class_name, class_name, 127);
    strncpy(native_table[native_count].method_name, method_name, 127);
    strncpy(native_table[native_count].descriptor, descriptor, 127);
    native_table[native_count].fn = fn;
    native_count++;
}

static native_method_fn find_native(const char *cn, const char *mn, const char *desc) {
    for (int i = 0; i < native_count; i++) {
        if (strcmp(native_table[i].class_name, cn) == 0 &&
            strcmp(native_table[i].method_name, mn) == 0 &&
            strcmp(native_table[i].descriptor, desc) == 0)
            return native_table[i].fn;
    }
    return NULL;
}

/* ============================================================
 * Bytecode interpreter (simplified)
 * ============================================================ */

extern void midp_init_natives(jvm_instance *jvm);

jvm_value jvm_execute_method(jvm_instance *jvm, jvm_class *class_info,
                              method_info *method, jvm_value *args, int argc) {
    if (!method || !method->code) {
        /* Try native */
        char *cname = cp_utf8(class_info, class_info->this_class);
        char *mname = cp_utf8(class_info, method->name_index);
        char *desc  = cp_utf8(class_info, method->descriptor_index);
        if (cname && mname && desc) {
            native_method_fn native = find_native(cname, mname, desc);
            if (native) {
                jvm_thread *thread = &jvm->threads[0];
                native(jvm, thread);
                if (thread->frames && thread->frames->stack_top >= 0)
                    return jvm_stack_pop(thread);
            }
        }
        jvm_value v; memset(&v, 0, sizeof(v)); return v;
    }

    /* Find or create thread */
    jvm_thread *thread = &jvm->threads[0];
    thread->running = 1;
    thread->frames = NULL;
    thread->stack_used = 0;

    /* Setup initial frame */
    jvm_frame *frame = (jvm_frame*)(thread->stack_data + thread->stack_used);
    thread->stack_used += sizeof(jvm_frame);
    memset(frame, 0, sizeof(jvm_frame));
    frame->method = method;
    frame->class_info = class_info;
    frame->pc = method->code;
    frame->max_stack = method->max_stack;
    frame->max_locals = method->max_locals;
    frame->prev = NULL;

    /* Allocate locals on the thread stack */
    size_t locals_size = method->max_locals * sizeof(jvm_value);
    size_t stack_size  = method->max_stack * sizeof(jvm_value);
    if (thread->stack_used + locals_size + stack_size > JVM_STACK_SIZE) return make_int(0);
    frame->locals = (jvm_value*)(thread->stack_data + thread->stack_used);
    thread->stack_used += locals_size;
    frame->stack = (jvm_value*)(thread->stack_data + thread->stack_used);
    thread->stack_used += stack_size;
    memset(frame->locals, 0, locals_size);
    memset(frame->stack, 0, stack_size);

    /* Pass arguments */
    for (int i = 0; i < argc && i < method->max_locals; i++)
        frame->locals[i] = args[i];
    frame->stack_top = -1;

    thread->frames = frame;

    /* Interpreter loop */
    u1 *pc;
    jvm_value result;
    memset(&result, 0, sizeof(result));

    #define READ_U1() (*pc++)
    #define READ_U2() (u2)((pc += 2, ((u2)(pc[-2] << 8) | pc[-1])))
    #define READ_U4() (u4)((pc += 4, ((u4)(pc[-4] << 24) | (u4)(pc[-3] << 16) | (u4)(pc[-2] << 8) | pc[-1])))

    int running = 1;
    while (running) {
        pc = frame->pc;
        u1 opcode = READ_U1();

        switch (opcode) {
            case 0x00: break; /* nop */

            /* Constants */
            case 0x01: { jvm_value v; v.ref = NULL; jvm_stack_push(thread, v); break; } /* aconst_null */
            case 0x02: jvm_stack_push(thread, make_int(-1)); break; /* iconst_m1 */
            case 0x03: jvm_stack_push(thread, make_int(0)); break;
            case 0x04: jvm_stack_push(thread, make_int(1)); break;
            case 0x05: jvm_stack_push(thread, make_int(2)); break;
            case 0x06: jvm_stack_push(thread, make_int(3)); break;
            case 0x07: jvm_stack_push(thread, make_int(4)); break;
            case 0x08: jvm_stack_push(thread, make_int(5)); break;
            case 0x09: jvm_stack_push(thread, make_int((s8)0)); break; /* lconst_0 */
            case 0x0a: jvm_stack_push(thread, make_int((s8)1)); break;
            case 0x0b: { jvm_value v; v.f = 0.0f; jvm_stack_push(thread, v); break; }
            case 0x0c: { jvm_value v; v.f = 1.0f; jvm_stack_push(thread, v); break; }
            case 0x0d: { jvm_value v; v.f = 2.0f; jvm_stack_push(thread, v); break; }
            case 0x0e: { jvm_value v; v.d = 0.0; jvm_stack_push(thread, v); break; }
            case 0x0f: { jvm_value v; v.d = 1.0; jvm_stack_push(thread, v); break; }

            case 0x10: { /* bipush */
                s1 b = (s1)READ_U1();
                jvm_stack_push(thread, make_int(b));
                break;
            }
            case 0x11: { /* sipush */
                s2 s = (s2)READ_U2();
                jvm_stack_push(thread, make_int(s));
                break;
            }
            case 0x12: { /* ldc */
                u2 idx = READ_U1();
                cp_info *cp = &class_info->cp[idx];
                if (cp->tag == CONSTANT_Integer) jvm_stack_push(thread, make_int(cp->data.integer));
                else if (cp->tag == CONSTANT_Float) { jvm_value v; v.f = cp->data.float_val; jvm_stack_push(thread, v); }
                else if (cp->tag == CONSTANT_String) {
                    char *str = cp_utf8(class_info, cp->data.string_ref.string_index);
                    jvm_stack_push(thread, make_ref(str));
                } else if (cp->tag == CONSTANT_Class) {
                    jvm_stack_push(thread, make_ref(NULL));
                }
                break;
            }
            case 0x13: case 0x14: { /* ldc_w, ldc2_w */
                u2 idx = READ_U2();
                cp_info *cp = &class_info->cp[idx];
                if (cp->tag == CONSTANT_Integer) jvm_stack_push(thread, make_int(cp->data.integer));
                else if (cp->tag == CONSTANT_Float) { jvm_value v; v.f = cp->data.float_val; jvm_stack_push(thread, v); }
                else if (cp->tag == CONSTANT_Long) jvm_stack_push(thread, make_int(cp->data.long_val));
                else if (cp->tag == CONSTANT_Double) { jvm_value v; v.d = cp->data.double_val; jvm_stack_push(thread, v); }
                else if (cp->tag == CONSTANT_String) { char *str = cp_utf8(class_info, cp->data.string_ref.string_index); jvm_stack_push(thread, make_ref(str)); }
                break;
            }

            /* Loads */
            case 0x15: { u2 idx = READ_U1(); jvm_stack_push(thread, jvm_local_get(thread, idx)); break; } /* iload */
            case 0x16: { u2 idx = READ_U1(); jvm_stack_push(thread, jvm_local_get(thread, idx)); break; } /* lload */
            case 0x17: { u2 idx = READ_U1(); jvm_stack_push(thread, jvm_local_get(thread, idx)); break; } /* fload */
            case 0x18: { u2 idx = READ_U1(); jvm_stack_push(thread, jvm_local_get(thread, idx)); break; } /* dload */
            case 0x19: { u2 idx = READ_U1(); jvm_stack_push(thread, jvm_local_get(thread, idx)); break; } /* aload */
            case 0x1a: case 0x1b: case 0x1c: case 0x1d: /* iload_0-3 */
                jvm_stack_push(thread, jvm_local_get(thread, opcode - 0x1a)); break;
            case 0x1e: case 0x1f: case 0x20: case 0x21: /* lload_0-3 */
                jvm_stack_push(thread, jvm_local_get(thread, opcode - 0x1e)); break;
            case 0x22: case 0x23: case 0x24: case 0x25: /* fload_0-3 */
                jvm_stack_push(thread, jvm_local_get(thread, opcode - 0x22)); break;
            case 0x26: case 0x27: case 0x28: case 0x29: /* dload_0-3 */
                jvm_stack_push(thread, jvm_local_get(thread, opcode - 0x26)); break;
            case 0x2a: case 0x2b: case 0x2c: case 0x2d: /* aload_0-3 */
                jvm_stack_push(thread, jvm_local_get(thread, opcode - 0x2a)); break;
            case 0x2e: { /* iaload */
                s4 idx = jvm_stack_pop(thread).i;
                jvm_array *arr = (jvm_array*)jvm_stack_pop(thread).ref;
                if (arr && idx >= 0 && idx < arr->length)
                    jvm_stack_push(thread, make_int(((s4*)arr->elements)[idx]));
                else
                    jvm_stack_push(thread, make_int(0));
                break;
            }

            /* Stores */
            case 0x36: { u2 idx = READ_U1(); jvm_local_set(thread, idx, jvm_stack_pop(thread)); break; } /* istore */
            case 0x37: { u2 idx = READ_U1(); jvm_local_set(thread, idx, jvm_stack_pop(thread)); break; } /* lstore */
            case 0x38: { u2 idx = READ_U1(); jvm_local_set(thread, idx, jvm_stack_pop(thread)); break; } /* fstore */
            case 0x39: { u2 idx = READ_U1(); jvm_local_set(thread, idx, jvm_stack_pop(thread)); break; } /* dstore */
            case 0x3a: { u2 idx = READ_U1(); jvm_local_set(thread, idx, jvm_stack_pop(thread)); break; } /* astore */
            case 0x3b: case 0x3c: case 0x3d: case 0x3e: /* istore_0-3 */
                jvm_local_set(thread, opcode - 0x3b, jvm_stack_pop(thread)); break;

            /* Stack */
            case 0x57: jvm_stack_pop(thread); break; /* pop */
            case 0x58: jvm_stack_pop(thread); jvm_stack_pop(thread); break; /* pop2 */
            case 0x59: { /* dup */
                jvm_value v = jvm_stack_pop(thread);
                jvm_stack_push(thread, v);
                jvm_stack_push(thread, v);
                break;
            }
            case 0x5a: { /* dup_x1 */
                jvm_value v1 = jvm_stack_pop(thread);
                jvm_value v2 = jvm_stack_pop(thread);
                jvm_stack_push(thread, v1);
                jvm_stack_push(thread, v2);
                jvm_stack_push(thread, v1);
                break;
            }
            case 0x5b: { /* dup_x2 */
                jvm_value v1 = jvm_stack_pop(thread);
                jvm_value v2 = jvm_stack_pop(thread);
                jvm_value v3 = jvm_stack_pop(thread);
                jvm_stack_push(thread, v1);
                jvm_stack_push(thread, v3);
                jvm_stack_push(thread, v2);
                jvm_stack_push(thread, v1);
                break;
            }
            case 0x5c: { /* dup2 */
                jvm_value v1 = jvm_stack_pop(thread);
                jvm_value v2 = jvm_stack_pop(thread);
                jvm_stack_push(thread, v2);
                jvm_stack_push(thread, v1);
                jvm_stack_push(thread, v2);
                jvm_stack_push(thread, v1);
                break;
            }
            case 0x5f: { /* swap */
                jvm_value v1 = jvm_stack_pop(thread);
                jvm_value v2 = jvm_stack_pop(thread);
                jvm_stack_push(thread, v1);
                jvm_stack_push(thread, v2);
                break;
            }

            /* Math */
            case 0x60: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; jvm_stack_push(thread, make_int(v1 + v2)); break; }
            case 0x64: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; jvm_stack_push(thread, make_int(v1 - v2)); break; }
            case 0x68: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; jvm_stack_push(thread, make_int(v1 * v2)); break; }
            case 0x6c: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i;
                if (v2 != 0) jvm_stack_push(thread, make_int(v1 / v2));
                else jvm_stack_push(thread, make_int(0));
                break;
            }
            case 0x70: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i;
                if (v2 != 0) jvm_stack_push(thread, make_int(v1 % v2));
                else jvm_stack_push(thread, make_int(0));
                break;
            }
            case 0x74: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; jvm_stack_push(thread, make_int(v1 + v2)); break; } /* ladd(ish) - simplified */
            case 0x78: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; jvm_stack_push(thread, make_int(v1 - v2)); break; }
            case 0x7c: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; jvm_stack_push(thread, make_int(v1 * v2)); break; }
            case 0x80: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; if (v2) jvm_stack_push(thread, make_int(v1 / v2)); else jvm_stack_push(thread, make_int(0)); break; }
            case 0x84: { /* iinc */
                u2 idx = READ_U1();
                s1 cnst = (s1)READ_U1();
                jvm_value v = jvm_local_get(thread, idx);
                v.i += cnst;
                jvm_local_set(thread, idx, v);
                break;
            }

            /* Casts */
            case 0x91: { s4 v = jvm_stack_pop(thread).i; jvm_stack_push(thread, make_int((s8)v)); break; } /* i2l */
            case 0x92: { s4 v = jvm_stack_pop(thread).i; jvm_value r; r.f = (float)v; jvm_stack_push(thread, r); break; } /* i2f */
            case 0x93: { jvm_value r; r.d = (double)jvm_stack_pop(thread).i; jvm_stack_push(thread, r); break; } /* i2d */

            /* Comparisons */
            case 0x94: { s8 v2 = jvm_stack_pop(thread).i; s8 v1 = jvm_stack_pop(thread).i;
                jvm_stack_push(thread, make_int(v1 > v2 ? 1 : (v1 == v2 ? 0 : -1))); break; } /* lcmp */
            case 0x95: { float v2 = jvm_stack_pop(thread).f; float v1 = jvm_stack_pop(thread).f;
                jvm_stack_push(thread, make_int(v1 > v2 ? 1 : (v1 == v2 ? 0 : -1))); break; } /* fcmpl */
            case 0x96: { float v2 = jvm_stack_pop(thread).f; float v1 = jvm_stack_pop(thread).f;
                jvm_stack_push(thread, make_int(v1 > v2 ? 1 : (v1 == v2 ? 0 : -1))); break; } /* fcmpg */
            case 0x97: { double v2 = jvm_stack_pop(thread).d; double v1 = jvm_stack_pop(thread).d;
                jvm_stack_push(thread, make_int(v1 > v2 ? 1 : (v1 == v2 ? 0 : -1))); break; } /* dcmpl */
            case 0x98: { double v2 = jvm_stack_pop(thread).d; double v1 = jvm_stack_pop(thread).d;
                jvm_stack_push(thread, make_int(v1 > v2 ? 1 : (v1 == v2 ? 0 : -1))); break; } /* dcmpg */

            /* Branches */
            case 0x99: { s2 off99 = (s2)READ_U2(); if (jvm_stack_pop(thread).i == 0) pc += off99; else pc += 2; break; } /* ifeq */
            case 0x9a: { s2 off9a = (s2)READ_U2(); if (jvm_stack_pop(thread).i != 0) pc += off9a; else pc += 2; break; } /* ifne */
            case 0x9b: { s2 off9b = (s2)READ_U2(); if (jvm_stack_pop(thread).i < 0) pc += off9b; else pc += 2; break; } /* iflt */
            case 0x9c: { s2 off9c = (s2)READ_U2(); if (jvm_stack_pop(thread).i >= 0) pc += off9c; else pc += 2; break; } /* ifge */
            case 0x9d: { s2 off9d = (s2)READ_U2(); if (jvm_stack_pop(thread).i > 0) pc += off9d; else pc += 2; break; } /* ifgt */
            case 0x9e: { s2 off9e = (s2)READ_U2(); if (jvm_stack_pop(thread).i <= 0) pc += off9e; else pc += 2; break; } /* ifle */
            case 0x9f: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; s2 off9f = (s2)READ_U2(); if (v1 == v2) pc += off9f; else pc += 2; break; } /* if_icmpeq */
            case 0xa0: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; s2 offa0 = (s2)READ_U2(); if (v1 != v2) pc += offa0; else pc += 2; break; }
            case 0xa1: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; s2 offa1 = (s2)READ_U2(); if (v1 < v2) pc += offa1; else pc += 2; break; }
            case 0xa2: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; s2 offa2 = (s2)READ_U2(); if (v1 >= v2) pc += offa2; else pc += 2; break; }
            case 0xa3: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; s2 offa3 = (s2)READ_U2(); if (v1 > v2) pc += offa3; else pc += 2; break; }
            case 0xa4: { s4 v2 = jvm_stack_pop(thread).i; s4 v1 = jvm_stack_pop(thread).i; s2 offa4 = (s2)READ_U2(); if (v1 <= v2) pc += offa4; else pc += 2; break; }
            case 0xa5: { void *v2 = jvm_stack_pop(thread).ref; void *v1 = jvm_stack_pop(thread).ref; s2 offa5 = (s2)READ_U2(); if (v1 == v2) pc += offa5; else pc += 2; break; } /* if_acmpeq */
            case 0xa6: { void *v2 = jvm_stack_pop(thread).ref; void *v1 = jvm_stack_pop(thread).ref; s2 offa6 = (s2)READ_U2(); if (v1 != v2) pc += offa6; else pc += 2; break; } /* if_acmpne */

            case 0xa7: { /* goto */
                s2 off_a7 = (s2)READ_U2();
                pc += off_a7;
                break;
            }
            case 0xa8: { /* jsr */
                jvm_value v; v.i = (s4)(uintptr_t)(pc + 2);
                jvm_stack_push(thread, v);
                s2 off_a8 = (s2)READ_U2();
                pc += off_a8;
                break;
            }
            case 0xa9: { /* ret */
                u2 idx = READ_U1();
                pc = method->code + jvm_local_get(thread, idx).i;
                break;
            }

            /* Switch */
            case 0xaa: { /* tableswitch */
                while (((uintptr_t)(pc - 1) & 3)) pc++;
                s4 default_off = (s4)READ_U4();
                s4 low = (s4)READ_U4();
                s4 high = (s4)READ_U4();
                s4 index = jvm_stack_pop(thread).i;
                if (index < low || index > high) pc += default_off;
                else { pc += (s4)(uintptr_t)&((u4*)pc)[index - low]; }
                break;
            }
            case 0xab: { /* lookupswitch */
                while (((uintptr_t)(pc - 1) & 3)) pc++;
                s4 default_off = (s4)READ_U4();
                s4 npairs = (s4)READ_U4();
                s4 key = jvm_stack_pop(thread).i;
                s4 match = 0;
                for (s4 i = 0; i < npairs; i++) {
                    s4 k = (s4)READ_U4();
                    s4 off = (s4)READ_U4();
                    if (k == key) { match = off; }
                }
                if (match) pc += match; else pc += default_off;
                break;
            }

            /* Returns */
            case 0xac: result = jvm_stack_pop(thread); running = 0; break; /* ireturn */
            case 0xad: result = jvm_stack_pop(thread); running = 0; break; /* lreturn */
            case 0xae: result = jvm_stack_pop(thread); running = 0; break; /* freturn */
            case 0xaf: result = jvm_stack_pop(thread); running = 0; break; /* dreturn */
            case 0xb0: result = jvm_stack_pop(thread); running = 0; break; /* areturn */
            case 0xb1: running = 0; break; /* return */

            /* Field access */
            case 0xb2: { /* getstatic */
                u2 idx = READ_U2();
                cp_info *cp = &class_info->cp[idx];
                jvm_class *fcls = (jvm_class*)(uintptr_t)cp->data.member_ref.class_index; /* simplified */
                (void)fcls;
                jvm_stack_push(thread, make_int(0));
                break;
            }
            case 0xb3: { /* putstatic */
                jvm_stack_pop(thread);
                READ_U2();
                break;
            }
            case 0xb4: { /* getfield */
                jvm_stack_pop(thread);
                READ_U2();
                jvm_stack_push(thread, make_int(0));
                break;
            }
            case 0xb5: { /* putfield */
                jvm_stack_pop(thread);
                jvm_stack_pop(thread);
                READ_U2();
                break;
            }

            /* Method invocation */
            case 0xb6: { /* invokevirtual */
                u2 idx = READ_U2();
                cp_info *cp = &class_info->cp[idx];
                cp_info *nat = &class_info->cp[cp->data.member_ref.nat_index];
                char *mname = cp_utf8(class_info, nat->data.name_and_type.name_index);
                char *desc  = cp_utf8(class_info, nat->data.name_and_type.desc_index);
                (void)mname; (void)desc;
                jvm_stack_pop(thread); /* pop object ref */
                jvm_stack_push(thread, make_int(0));
                break;
            }
            case 0xb7: { /* invokespecial */
                u2 idx = READ_U2();
                cp_info *cp = &class_info->cp[idx];
                cp_info *nat = &class_info->cp[cp->data.member_ref.nat_index];
                char *mname = cp_utf8(class_info, nat->data.name_and_type.name_index);
                char *desc  = cp_utf8(class_info, nat->data.name_and_type.desc_index);
                char *cname = cp_utf8(class_info, cp->data.member_ref.class_index);

                if (cname && mname && desc) {
                    native_method_fn native = find_native(cname, mname, desc);
                    if (native) {
                        native(jvm, thread);
                    }
                }
                break;
            }
            case 0xb8: { /* invokestatic */
                u2 idx = READ_U2();
                cp_info *cp = &class_info->cp[idx];
                cp_info *nat = &class_info->cp[cp->data.member_ref.nat_index];
                cp_info *cls_ref = &class_info->cp[cp->data.member_ref.class_index];
                char *mname = cp_utf8(class_info, nat->data.name_and_type.name_index);
                char *desc  = cp_utf8(class_info, nat->data.name_and_type.desc_index);
                char *cname = cp_utf8(class_info, cls_ref->data.class_ref.class_index);

                if (cname && mname && desc) {
                    native_method_fn native = find_native(cname, mname, desc);
                    if (native) {
                        native(jvm, thread);
                    } else {
                        /* Try to find <clinit> */
                        jvm_class *target = jvm_find_class(jvm, cname);
                        if (target) {
                            for (u2 m = 0; m < target->methods_count; m++) {
                                char *tm = cp_utf8(target, target->methods[m].name_index);
                                if (tm && strcmp(tm, mname) == 0) {
                                    break;
                                }
                            }
                        }
                        jvm_stack_push(thread, make_int(0));
                    }
                } else {
                    jvm_stack_push(thread, make_int(0));
                }
                break;
            }
            case 0xb9: { /* invokeinterface */
                READ_U2(); READ_U1(); /* skip interface method ref + count */
                jvm_stack_pop(thread);
                jvm_stack_push(thread, make_int(0));
                break;
            }
            case 0xbb: { /* new */
                u2 idx = READ_U2();
                cp_info *cp = &class_info->cp[idx];
                cp_info *cls_cp = &class_info->cp[cp->data.class_ref.class_index];
                char *cname = cp_utf8(class_info, cls_cp->data.class_ref.class_index);
                jvm_class *target = jvm_find_class(jvm, cname);
                if (target) {
                    jvm_object *obj = jvm_alloc_object(jvm, target);
                    jvm_stack_push(thread, make_ref(obj));
                } else {
                    jvm_stack_push(thread, make_ref(NULL));
                }
                break;
            }
            case 0xbe: { /* arraylength */
                jvm_array *arr = (jvm_array*)jvm_stack_pop(thread).ref;
                if (arr) jvm_stack_push(thread, make_int(arr->length));
                else jvm_stack_push(thread, make_int(0));
                break;
            }
            case 0xbf: { /* athrow */
                jvm_stack_pop(thread);
                running = 0;
                printf("jvm: exception thrown\n");
                break;
            }

            /* Object operations */
            case 0xc0: { /* checkcast */
                jvm_stack_pop(thread);
                jvm_stack_push(thread, make_int(0));
                READ_U2();
                break;
            }
            case 0xc1: { /* instanceof */
                jvm_stack_pop(thread);
                jvm_stack_push(thread, make_int(0));
                READ_U2();
                break;
            }
            case 0xc6: { /* ifnull */
                s2 off_c6 = (s2)READ_U2();
                if (jvm_stack_pop(thread).ref == NULL) pc += off_c6; else pc += 2;
                break;
            }
            case 0xc7: { /* ifnonnull */
                s2 off_c7 = (s2)READ_U2();
                if (jvm_stack_pop(thread).ref != NULL) pc += off_c7; else pc += 2;
                break;
            }

            /* Wide */
            case 0xc4: {
                u2 opc2 = READ_U1();
                u2 idx = READ_U2();
                switch (opc2) {
                    case 0x15: case 0x16: case 0x17: case 0x18: case 0x19:
                        jvm_stack_push(thread, jvm_local_get(thread, idx)); break;
                    case 0x36: case 0x37: case 0x38: case 0x39: case 0x3a:
                        jvm_local_set(thread, idx, jvm_stack_pop(thread)); break;
                    case 0x84: {
                        s2 cnst = (s2)READ_U2();
                        jvm_value v = jvm_local_get(thread, idx);
                        v.i += cnst;
                        jvm_local_set(thread, idx, v);
                        break;
                    }
                    case 0xa9: pc = method->code + jvm_local_get(thread, idx).i; break;
                }
                break;
            }

            /* Multianewarray */
            case 0xc5: {
                READ_U2();
                u1 dims = READ_U1();
                for (u1 i = 0; i < dims; i++) jvm_stack_pop(thread);
                jvm_stack_push(thread, make_ref(NULL));
                break;
            }

            default:
                printf("jvm: unsupported opcode 0x%02X at offset %d\n", opcode, (int)(pc - 1 - method->code));
                running = 0;
                break;
        }

        frame->pc = pc;
    }

    thread->running = 0;
    return result;
}

/* ============================================================
 * JVM lifecycle
 * ============================================================ */

jvm_instance *jvm_create(void) {
    jvm_instance *jvm = (jvm_instance*)calloc(1, sizeof(jvm_instance));
    if (!jvm) return NULL;
    memset(jvm, 0, sizeof(jvm_instance));
    midp_init_natives(jvm);
    return jvm;
}

void jvm_destroy(jvm_instance *jvm) {
    if (!jvm) return;
    /* Free class data */
    jvm_class *c = jvm->classes;
    while (c) {
        jvm_class *next = c->next;
        if (c->cp) {
            for (u2 i = 1; i < c->cp_count; i++) {
                if (c->cp[i].tag == CONSTANT_Utf8)
                    free((void*)(uintptr_t)c->cp[i].data.utf8.string_index);
            }
            free(c->cp);
        }
        for (u2 i = 0; i < c->methods_count; i++)
            free(c->methods[i].code);
        free(c->methods);
        free(c->fields);
        free(c->interfaces);
        free(c->static_data);
        free(c);
        c = next;
    }
    free(jvm);
}

void jvm_run(jvm_instance *jvm, const char *midlet_class) {
    /* Convert "com.example.MyMIDlet" to "com/example/MyMIDlet" */
    char class_path[256];
    strncpy(class_path, midlet_class, 255);
    class_path[255] = 0;
    for (char *p = class_path; *p; p++)
        if (*p == '.') *p = '/';

    jvm_class *cls = jvm_find_class(jvm, class_path);
    if (!cls) {
        printf("jvm: MIDlet class '%s' not found\n", class_path);
        return;
    }

    /* Find startApp method */
    method_info *start_app = NULL;
    for (u2 i = 0; i < cls->methods_count; i++) {
        char *mname = cp_utf8(cls, cls->methods[i].name_index);
        if (mname && strcmp(mname, "startApp") == 0) {
            start_app = &cls->methods[i];
            break;
        }
    }

    if (start_app) {
        jvm_value args[1];
        args[0] = make_ref(NULL);
        jvm_execute_method(jvm, cls, start_app, args, 0);
    }

    /* Find pauseApp and destroyApp similarly */
}

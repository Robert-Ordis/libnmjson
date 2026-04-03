#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "include/nmjson/nmjson.h"
#include "include/nmjson/nmjson_writer.h"
#include "include/nmjson/nmjson_buffer.h"

#define TOKENS_NUM 512
#define CBUF_NUM 4096

static const nmjson_str_t k_key_unicode_head = NMJSON_STR_LITERAL("🙂key中tail");
static const nmjson_str_t k_key_unicode_mid = NMJSON_STR_LITERAL("headΩ🙂終端");
static const nmjson_str_t k_key_ctrl_mix = NMJSON_STR_LITERAL("\nkey中\t🙂tail\r");
static const nmjson_str_t k_key_ctrl_edges = NMJSON_STR_LITERAL("\b先頭mix終端🙃\x01");

struct expected_s {
    int64_t iv;
    double dbl;
    const char *str;
    int boolean;
    const char *unicode_head;
    const char *unicode_mid;
    const char *unicode_tail;
    const char *ctrl_mix;
    const char *ctrl_edges;
};

static struct expected_s expected = {
    .iv = 12345,
    .dbl = 3.14159,
    .str = "hello world",
    .boolean = 1,
    .unicode_head = "あAlpha中Beta🙂",
    .unicode_mid = "ASCII🙂中間Ωtail",
    .unicode_tail = "head123終端🙃",
    .ctrl_mix = "\bAあ\tB🙂\nC終\r",
    .ctrl_edges = "\n先頭ASCII中盤\x01終端🙃\t"
};

static const char *k_complex_key_values[] = {
    "value-for-unicode-head-key",
    "value-for-unicode-mid-key",
    "value-for-ctrl-mix-key",
    "value-for-ctrl-edges-key"
};

static const char *k_array_expected[] = {
    "ASCII only",
    "🙂arr中間Ωtail",
    "head123終端🙃",
    "\bAあ\tB🙂\nC終\r",
    "\n先頭ASCII中盤\x01終端🙃\t"
};

static const nmjson_token_t *find_object_child_n(const nmjson_token_t *obj, const nmjson_str_t *name){
    const nmjson_token_t *child;

    if(obj == NULL || obj->v.type != nmjson_type_object){
        return NULL;
    }

    for(child = nmjson_token_head_child(obj); child != NULL; child = nmjson_token_next(child)){
        if(child->n.len == name->len && memcmp(child->n.name, name->s, name->len) == 0){
            return child;
        }
    }
    return NULL;
}

static void assert_string_token(const nmjson_token_t *token, const char *label, const char *expected_value){
    const char *got;

    if(token == NULL || token->v.type != nmjson_type_string){
        fprintf(stderr, "%s missing or not string\n", label);
        exit(1);
    }

    got = nmjson_token_as_string(token, NULL);
    if(got == NULL || strcmp(got, expected_value) != 0){
        fprintf(stderr, "%s mismatch: got=%s expected=%s\n", label, got ? got : "(null)", expected_value);
        exit(1);
    }
}

static void assert_string_field(const nmjson_token_t *token_root, const char *key, const char *expected_value){
    const char *got = nmjson_object_get_string(token_root, key, NULL);
    if(got == NULL || strcmp(got, expected_value) != 0){
        fprintf(stderr, "%s mismatch: got=%s expected=%s\n", key, got ? got : "(null)", expected_value);
        exit(1);
    }
}

static int dispatcher_validate(const nmjson_token_t *token_root, nmjson_error_t error, int fd, void *arg){
    (void)fd; (void)arg;
    if(error != nmjson_error_complete){
        fprintf(stderr, "parse error: %d\n", error);
        return 1; /* stop */
    }

    if(token_root->v.type != nmjson_type_object){
        fprintf(stderr, "root is not object\n");
        exit(1);
    }
	printf("json has been read as below:\n");
    nmjson_token_fout2(token_root, stdout, 1, 1);
	nmjson_token_fout2(token_root, stdout, 0, 0);
    int64_t got_iv = nmjson_object_get_int(token_root, "iv", -1);
    if(got_iv != expected.iv){
        fprintf(stderr, "iv mismatch: got=%" PRId64 " expected=%" PRId64 "\n", got_iv, expected.iv);
        exit(1);
    }

    double got_dbl = nmjson_object_get_float(token_root, "dbl", -1.0);
    if(fabs(got_dbl - expected.dbl) > 1e-6){
        fprintf(stderr, "dbl mismatch: got=%f expected=%f\n", got_dbl, expected.dbl);
        exit(1);
    }

    assert_string_field(token_root, "str", expected.str);
    assert_string_field(token_root, "unicode_head", expected.unicode_head);
    assert_string_field(token_root, "unicode_mid", expected.unicode_mid);
    assert_string_field(token_root, "unicode_tail", expected.unicode_tail);
    assert_string_field(token_root, "ctrl_mix", expected.ctrl_mix);
    assert_string_field(token_root, "ctrl_edges", expected.ctrl_edges);
    assert_string_token(find_object_child_n(token_root, &k_key_unicode_head), "complex key unicode head", k_complex_key_values[0]);
    assert_string_token(find_object_child_n(token_root, &k_key_unicode_mid), "complex key unicode mid", k_complex_key_values[1]);
    assert_string_token(find_object_child_n(token_root, &k_key_ctrl_mix), "complex key ctrl mix", k_complex_key_values[2]);
    assert_string_token(find_object_child_n(token_root, &k_key_ctrl_edges), "complex key ctrl edges", k_complex_key_values[3]);

    if(!nmjson_object_is_true(token_root, "b")){
        fprintf(stderr, "bool mismatch\n");
        exit(1);
    }

    if(!nmjson_object_is_nullobj(token_root, "n")){
        fprintf(stderr, "null field mismatch\n");
        exit(1);
    }

    /* check array "arr" has 3 elements: 1,2,3 */
    const nmjson_token_t *arr = nmjson_object_get_array(token_root, "arr");
    if(!arr){
        fprintf(stderr, "arr missing\n");
        exit(1);
    }
    if((int)arr->v.len != 3){
        fprintf(stderr, "arr length mismatch: %zu\n", arr->v.len);
        exit(1);
    }
    for(int i = 0; i < 3; i++){
        const nmjson_token_t *elem = nmjson_token_nth_child(arr, i);
        if(!elem){ fprintf(stderr, "arr elem %d missing\n", i); exit(1); }
        int64_t val = nmjson_token_as_int(elem, -999);
        if(val != i+1){ fprintf(stderr, "arr elem %d mismatch: %" PRId64 "\n", i, val); exit(1); }
    }

    const nmjson_token_t *arr_str = nmjson_object_get_array(token_root, "arr_str");
    if(!arr_str){
        fprintf(stderr, "arr_str missing\n");
        exit(1);
    }
    if((int)arr_str->v.len != (int)(sizeof(k_array_expected) / sizeof(k_array_expected[0]))){
        fprintf(stderr, "arr_str length mismatch: %zu\n", arr_str->v.len);
        exit(1);
    }
    for(int i = 0; i < (int)(sizeof(k_array_expected) / sizeof(k_array_expected[0])); i++){
        const nmjson_token_t *elem = nmjson_token_nth_child(arr_str, i);
        char label[64];
        if(!elem){ fprintf(stderr, "arr_str elem %d missing\n", i); exit(1); }
        snprintf(label, sizeof(label), "arr_str elem %d", i);
        assert_string_token(elem, label, k_array_expected[i]);
    }

    /* nested object */
    const nmjson_token_t *nested = nmjson_object_get_object(token_root, "nested");
    if(!nested){ fprintf(stderr, "nested missing\n"); exit(1); }
    const char *kv = nmjson_object_get_string(nested, "k", NULL);
    if(!kv || strcmp(kv, "v") != 0){ fprintf(stderr, "nested.k mismatch\n"); exit(1); }

    printf("roundtrip: OK\n");
    return 1; /* tell reader loop we can stop */
}

void test20(){
    int pipefd[2] = { -1, -1 };
    if(pipe(pipefd) != 0){ perror("pipe"); exit(1); }

    /* writer: write to pipefd[1] */
    nmjson_writer_t writer;
    nmjson_writer_init_fd(&writer, pipefd[1]);
    nmjson_writer_cfg_pretty_print(&writer, 0);

    nmjson_writer_with_object(&writer, NULL, {
        nmjson_writer_put_int(&writer, "iv", expected.iv);
        nmjson_writer_put_float(&writer, "dbl", expected.dbl);
        nmjson_writer_put_string(&writer, "str", expected.str);
        nmjson_writer_put_string(&writer, "unicode_head", expected.unicode_head);
        nmjson_writer_put_string(&writer, "unicode_mid", expected.unicode_mid);
        nmjson_writer_put_string(&writer, "unicode_tail", expected.unicode_tail);
        nmjson_writer_put_string(&writer, "ctrl_mix", expected.ctrl_mix);
        nmjson_writer_put_string(&writer, "ctrl_edges", expected.ctrl_edges);
        nmjson_writer_put_string_n(&writer, &k_key_unicode_head, k_complex_key_values[0]);
        nmjson_writer_put_string_n(&writer, &k_key_unicode_mid, k_complex_key_values[1]);
        nmjson_writer_put_string_n(&writer, &k_key_ctrl_mix, k_complex_key_values[2]);
        nmjson_writer_put_string_n(&writer, &k_key_ctrl_edges, k_complex_key_values[3]);
        nmjson_writer_put_bool(&writer, "b", expected.boolean);
        nmjson_writer_put_nullobj(&writer, "n");
        nmjson_writer_with_array(&writer, "arr", {
            nmjson_writer_put_int(&writer, NULL, 1);
            nmjson_writer_put_int(&writer, NULL, 2);
            nmjson_writer_put_int(&writer, NULL, 3);
        });
        nmjson_writer_with_array(&writer, "arr_str", {
            size_t i;
            for(i = 0; i < sizeof(k_array_expected) / sizeof(k_array_expected[0]); i++){
                nmjson_writer_put_string(&writer, NULL, k_array_expected[i]);
            }
        });
        nmjson_writer_with_object(&writer, "nested", {
            nmjson_writer_put_string(&writer, "k", "v");
        });
    });

    /* close write end to signal EOF */
    close(pipefd[1]); pipefd[1] = -1;

    /* reader: read from pipefd[0] and validate */
    nmjson_buffer_t jbuffer;
    nmjson_token_t tokens[TOKENS_NUM];
    char cbuf[CBUF_NUM];

    nmjson_buffer_init(&jbuffer, tokens, TOKENS_NUM, cbuf, CBUF_NUM);

    int exret = 0;
    while((exret = nmjson_buffer_read(&jbuffer, dispatcher_validate, pipefd[0], NULL)) == 0){}

    if(pipefd[0] >= 0){ close(pipefd[0]); pipefd[0] = -1; }

    if(exret < 0){ fprintf(stderr, "reader returned error %d\n", exret); exit(1); }
}

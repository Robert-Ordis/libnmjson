# libnmjson

## No-Malloc-JSON

### mallocなし・再帰なしで、fd越しのJSON通信（受信・振り分け・返信）を完結させるCライブラリ

* ローカル変数だけで運用できます。トークンもバッファも呼び出し側が用意します（malloc版も選べます）
* fdから読んで、JSONが1個そろうたびにコールバック。`{"cmd":...}` の値で処理を振り分けることもできます
* 返信はfdへ直接書き出し。構造の組み立てはしない割り切った作りなので、メモリは出力の大きさに依存しません
* JSONC / JSON5 の拡張を**機能単位**で選べます（「JSONにコメントだけ許す」等）
* 数値は最短表現・ロケール非依存で読み書きします（`setlocale` の影響を受けません）

---

## ビルドとインストール

静的ライブラリ(`.a`)のみを生成します。`prefix` を付けてプロジェクトローカルに入れることを推奨します。

```
$ autoreconf -vfi
$ ./configure --prefix=/path/to/your/project && make && make install
```

* 組み込み向け: `./configure CFLAGS="-Os -DNDEBUG"`（x86_64で text 約25KB。-O2では約38KB）
* 自分の共有ライブラリ(`.so`)に組み込む場合: `CFLAGS="-fPIC"` を追加
* ライブラリを `CFLAGS="-ffunction-sections"` でビルドし、アプリのリンク時に `-Wl,--gc-sections` を付けると、使わない関数がさらに落ちます

ヘッダは用途ごとに分かれています。

```c
#include <nmjson/nmjson.h>          // パーサー・トークン(値の取り出し)
#include <nmjson/nmjson_buffer.h>   // fdからの受信・振り分け
#include <nmjson/nmjson_writer.h>   // 書き出し
#include <nmjson/nmjson_version.h>  // バージョン情報(NMJSON_VERSION_* / nmjson_get_build_ver())
```

バージョンの定義元は `configure.ac` 先頭の `nmjson_ver_*` だけです。`nmjson_version.h` は configure が生成します。

---

## 使い方: fdで受けて、振り分けて、返す

以下は要求 `{"cmd":"get","id":1}` を受けて、応答 `{"result":"ok","id":1,"temp":23.5}` を返すサーバーの骨格です。
ソケット・パイプ・ファイル等、`read(2)` / `write(2)` できるfdなら何でも構いません。

```c
#include <nmjson/nmjson.h>
#include <nmjson/nmjson_buffer.h>
#include <nmjson/nmjson_writer.h>

#define TOKENS_NUM  64      // 1メッセージあたりの値の数の上限
#define BUF_LEN     1024    // 1メッセージの長さの上限

/// "get" の処理: 受け取った fd へそのまま返信する
static int on_get(const nmjson_token_t *root, nmjson_error_t error, int fd, void *arg){
    nmjson_writer_t w;
    nmjson_writer_init_fd(&w, fd);
    nmjson_writer_with_object(&w, NULL, {
        nmjson_writer_put_string(&w, "result", "ok");
        nmjson_writer_put_int(&w, "id", nmjson_object_get_int(root, "id", -1));
        nmjson_writer_put_float(&w, "temp", 23.5);
    });
    return (w.err.num == 0) ? 0 : -1;
}

/// 未知のコマンド、または解析エラー(error != nmjson_error_complete)の時に呼ばれる
static int on_other(const nmjson_token_t *root, nmjson_error_t error, int fd, void *arg){
    nmjson_writer_t w;
    nmjson_writer_init_fd(&w, fd);
    nmjson_writer_with_object(&w, NULL, {
        nmjson_writer_put_string(&w, "result", "error");
        nmjson_writer_put_int(&w, "code", (int64_t)error);
    });
    return 0;
}

static nmjson_cmd_elem_t k_cmds[] = {
    { "get", on_get   },
    { NULL,  on_other },    // 末尾は name == NULL。ここがデフォルトの処理になる
};

void serve(int fd){
    nmjson_buffer_t jb;
    nmjson_token_t  tokens[TOKENS_NUM];
    char            buf[BUF_LEN];
    int ret;

    nmjson_buffer_init(&jb, tokens, TOKENS_NUM, buf, sizeof(buf));
    for(;;){
        ret = nmjson_buffer_read_for_cmd(&jb, k_cmds, "cmd", fd, NULL);
        if(ret == -1 || ret == -2){
            break;      // 切断 / システムエラー
        }
        // 0: 続きを読む / -3: そのメッセージは破棄済み(続行するかはアプリの判断)
    }
    nmjson_buffer_deinit(&jb);
}
```

振り分けが不要なら `nmjson_buffer_read` を使います。コールバックの形は同じです。

```c
static int on_json(const nmjson_token_t *root, nmjson_error_t error, int fd, void *arg){
    if(error != nmjson_error_complete){
        return 0;           // 構文エラー/トークン不足/バッファ不足。メッセージは破棄される
    }
    printf("id=%" PRId64 "\n", nmjson_object_get_int(root, "id", -1));
    return 0;               // 0: 読み続ける / 0以外: nmjson_buffer_read から戻る
}

    while((ret = nmjson_buffer_read(&jb, on_json, fd, NULL)) != -1 && ret != -2){}
```

### 受信の動き

* `read(2)` で読めた分だけ解析し、JSONが1個そろうたびにコールバックを呼びます。途中で切れていれば続きを待ちます
* `{"a":1}{"a":2}` のように連続して届くJSONを順に処理します（区切りは不要。空白・改行があってもよい）
* 1回の呼び出しで、読めた分に含まれるJSONを全て処理します
* ルートはオブジェクトか配列である必要があります

### 戻り値（`nmjson_buffer_read` / `nmjson_buffer_read_for_cmd`）

| 戻り値 | 意味 |
|---|---|
| `0` | 続きを読む必要がある。`EAGAIN` / `EWOULDBLOCK` / `EINTR` もここ |
| `1`以上 | コールバックが0以外を返した（「もう抜けてよい」） |
| `-1` | EOF（切断・ファイル終端） |
| `-2` | システムエラー（`errno` を参照） |
| `-3` | ライブラリ側のエラー（構文エラー・トークン不足・バッファ不足）。該当メッセージは破棄済みで、コールバックにはエラーコード付きで通知済み |
| `-4`以下 | コールバックが負の値を返した（`-3 + 戻り値`） |

### イベントループに組み込む場合

ノンブロッキングのfdを `poll` / `epoll` 等で監視し、読み込み可能になるたびに1回呼びます。
`0` 以上なら次のイベントを待ち、`-1` / `-2` なら切断処理をします。接続ごとに `nmjson_buffer_t` を1個用意してください。

### 返信（`nmjson_writer_t`）

* 出力先は fd（`nmjson_writer_init_fd`）、`FILE *`（`nmjson_writer_init_fp`）、任意のコールバック（`nmjson_writer_init_cb`）
* `nmjson_writer_with_object` / `nmjson_writer_with_array` で入れ子をブロック風に書けます
* 文字列のエスケープ、不正なUTF-8の扱い、数値の書式はライブラリが担当します
* **JSONとしての構造の正しさ（begin/endの対応など）は呼び出し側の責任です**。決まった形のメッセージをやりとりする用途を想定した割り切りで、その代わりメモリは出力の大きさに依存しません
* `init_fd` は書き出しの断片ごとに `write(2)` を呼びます。ソケットでシステムコールの回数を減らしたい場合は、`init_cb` で小さなバッファを挟んでください
* 設定: `nmjson_writer_cfg_pretty_print`（インデント・改行）、`nmjson_writer_cfg_utf8_raw`（`\uXXXX` にしない）、`nmjson_writer_cfg_superset`（後述）

---

## メモリの見積もり

mallocを使わない場合、必要なメモリは事前に確定します。

```
sizeof(nmjson_buffer_t) + トークン数 × sizeof(nmjson_token_t) + 受信バッファ長
```

* x86_64 での実測: `nmjson_token_t` 64、`nmjson_buffer_t` 152（パーサー込み）、`nmjson_parser_t` 80、`nmjson_writer_t` 64 バイト
* **トークン数 = 1メッセージ中の値の数**（ルート・オブジェクト・配列・スカラー値の全て）。キー名は値のトークンに同居するので数えません
  * 例: `{"cmd":"set","id":12345,"params":{"name":"s01","value":23.5,"enabled":true,"tags":["a","b","c"]}}` は11個
* **受信バッファ長 = 1メッセージの最大長 + 1**（終端用）
* 文字列はバッファ上でその場でエスケープ解除されるため、文字列用の領域は別に要りません（その代わりバッファの中身は書き換わります）
* 上限を超えた入力は `nmjson_error_tokens` / `nmjson_error_buffer` で安全に止まります。不正な相手から巨大な入力を送られても、メモリが際限なく増えることはありません
* 解析は再帰を使わないため、入れ子の深さに関係なくスタック消費は一定です

---

## メッセージの大きさが読めない場合: バッファの入れ替え

受信バッファは、コールバックで拡張・縮小できます。

| 方式 | 向いている場面 | メモリ |
|---|---|---|
| 固定長（コールバックなし） | 最大長をプロトコルで決められる（機器間の決まった形のやりとり） | 接続数 × 最大長 |
| 貸し出し（mallocなし） | 普段は小さいが、たまに大きいものが来る。接続数が多い | 接続数 × 小 ＋ 大 × 少数 |
| realloc（上限付き） | 大きさがまったく読めない | 実際に使った分 |

```c
/* 貸し出し版: 全接続で共有する大きいバッファを1枚だけ用意する */
static char g_big[16384];
static int  g_big_in_use = 0;

typedef struct {
    char    *small;         // 接続ごとの小さいバッファ(init時に渡したもの)
    size_t  small_len;
} conn_buf_t;

static char *lend_cb(nmjson_buffer_resize_t reason, char *cur, size_t cur_len,
    size_t used_len, size_t *new_len, void *arg)
{
    conn_buf_t *c = arg;
    switch(reason){
    case nmjson_buffer_resize_grow:         // 満杯で解析途中
        if(cur != c->small || g_big_in_use){
            return NULL;                    // 貸し出し中 → 諦める(nmjson_error_buffer)
        }
        g_big_in_use = 1;
        memcpy(g_big, cur, used_len + 1);   // 中身のコピーはコールバックの責務
        *new_len = sizeof(g_big);
        return g_big;
    case nmjson_buffer_resize_shrink:       // 1メッセージ処理し終えた
        if(cur != g_big || used_len + 2 > c->small_len){
            return NULL;
        }
        memcpy(c->small, cur, used_len + 1);
        g_big_in_use = 0;
        *new_len = c->small_len;
        return c->small;
    case nmjson_buffer_resize_release:      // nmjson_buffer_deinit
        if(cur == g_big){
            g_big_in_use = 0;
        }
        return NULL;
    }
    return NULL;
}

    conn_buf_t cb = { small, sizeof(small) };
    nmjson_buffer_init(&jb, tokens, TOKENS_NUM, small, sizeof(small));
    nmjson_buffer_set_resize_cb(&jb, lend_cb, &cb);     // init の後に設定する
```

realloc版では `grow` / `shrink` で `realloc` を、`release` で `free` を呼ぶだけです（初期バッファも malloc で用意します）。

コールバックの約束事:

* 新しいバッファの先頭 `used_len + 1` バイトに、元のバッファの内容を**そのまま**写すこと（`realloc` はこれを満たします）。解析済みの部分は書き換わっているので、受信した生データを入れ直してはいけません
* 戻った後、元のバッファはライブラリから参照されません。コールバック内で解放・返却して構いません
* `reason` は `grow` / `shrink` / `release` の3値です。`if(grow){...}else{...}` のように2値前提で書くと `release` を縮小として処理してしまうので、必ず個別に扱ってください
* 入れ替えの後は、コールバック内で受け取ったトークンから得た文字列ポインタを保持していても無効になります
* バッファを伸ばしても、トークンが足りなければ `nmjson_error_tokens` で止まります。トークン数も合わせて見積もってください

## 初期化と後始末

`init` と `deinit` は対で呼んでください。

```c
nmjson_buffer_init(&jb, tokens, TOKENS_NUM, buf, sizeof(buf));
/* ... */
nmjson_buffer_deinit(&jb);      // 内部パーサーの後始末 + resize_cb へ release 通知
```

* mallocなしで使う場合 `deinit` は実質何もしませんが、呼んでおけば後からmalloc版へ切り替えても利用側を直さずに済みます
* mallocありのパーサー（`nmjson_parser_init(&parser, NULL, 0)`）は、`nmjson_parser_deinit` を呼ぶまでトークンを解放しません
* 渡したトークン配列・初期の受信バッファは呼び出し側の持ち物で、ライブラリは解放しません

---

## JSON拡張（superset）

`nmjson_superset_t` は機能ごとのビットフィールドです。パーサーでは「受け入れる」、writerでは「出力してよい」を意味します。

| JSON5の拡張 | フィールド | jsonc | json5 | 例 |
|---|---|:-:|:-:|---|
| コメント | `comment` | ○ | ○ | `// ...` `/* ... */` |
| 末尾カンマ | `tailcomma` | ○ | ○ | `[1,2,]` |
| クォートなしのキー | `identkey` | | ○ | `{key: 1}` |
| シングルクォートの文字列 | `literalsq` | | ○ | `'str'` |
| 文字列中の行継続 | `linecontinuation` | | ○ | `"a\⏎b"` |
| エスケープの拡張 | `extescape` | | ○ | `\v` `\0` |
| 16進エスケープ | `hexaescape` | | ○ | `\x41` |
| 数値の拡張 | `extnum` | | ○ | `0x1F` `+1` `Infinity` `NaN` |
| 小数の省略表記 | `abbrdecimal` | | ○ | `.5` `5.` |
| 追加の空白文字 | `extspace` | | ○ | VT, FF, U+00A0, U+FEFF, U+3000, U+2028 等 |
| 文字列中の生の制御文字（独自拡張） | `literalcntl` | | | `"a⏎b"`（プリセット `nmjson_superset_rough`） |

```c
nmjson_superset_t s = nmjson_superset_none;   // 拡張なし(RFC 8259)
s.comment = 1;                                  // JSON + コメントだけ
nmjson_buffer_init_superset(&jb, tokens, TOKENS_NUM, buf, sizeof(buf), s);

s = nmjson_superset_jsonc;                      // プリセットに足し引きもできる
s.literalsq = 1;
```

* プリセット: `nmjson_superset_none` / `_jsonc` / `_json5` / `_rough`（C99の複合リテラル）
* writerが現在参照するのは `extnum`（NaN/Infinityを素の値で出す）と、コメント出力の条件（`comment` かつ `tailcomma`、かつ pretty_print）です
* 拡張なしの場合、空白は RFC 8259 の4文字（SP, TAB, LF, CR）のみです

---

## 値の取り出し

```c
const nmjson_token_t *root = ...;   // コールバックの引数、または nmjson_parser_get_root()

int64_t     id   = nmjson_object_get_int(root, "id", -1);          // 第3引数は見つからない時の既定値
double      temp = nmjson_object_get_float(root, "temp", 0.0);
const char *name = nmjson_object_get_string(root, "name", "");
int         on   = nmjson_object_is_true(root, "enabled");
const nmjson_token_t *arr = nmjson_object_get_array(root, "tags");
const nmjson_token_t *c;
for(c = nmjson_token_head_child(arr); c != NULL; c = nmjson_token_next(c)){ ... }
```

* キー名は `n.name` / `n.len`、値は `v.type` / `v.value` / `v.len` で直接参照することもできます
* **クォートなしのキー（JSON5）の `n.name` は `'\0'` 終端されません**。`n.len` で長さを扱ってください
* オブジェクトのキーは解析後に並べ替えられます（元の順序は保持されません）

## fdを使わずに解析する

```c
char src[] = "{\"args\":\"TestString\",\"argi\":114514}";  // 書き換え可能なバッファであること
nmjson_parser_t parser;
nmjson_token_t  tokens[16];

nmjson_parser_init(&parser, tokens, 16);
nmjson_parser_parse(&parser, src);
if(parser.error.code == nmjson_error_complete){
    const nmjson_token_t *root = nmjson_parser_get_root(&parser);
    /* ... */
}
nmjson_parser_deinit(&parser);
```

* 入力はその場で書き換えられます。**文字列リテラルを直接渡すとセグフォります**
* 途中までのデータを渡すと `nmjson_error_incomplete` が返ります。同じバッファの末尾に続きを足して再度 `nmjson_parser_parse` を呼ぶと、続きから解析します

---

## 制約・既知の未対応点

* JSON5のクォートなしキーは、終わりの判定（`:`・コメント・空白）は厳密ですが、識別子として使える文字の判定は緩くしています（`{a-b:1}` も受け入れる）。正しいJSON5は正しく読めます。writerはキーを必ずクォートして出力するので、出力側の相互運用性には影響しません
  →識別子の文字を厳密に判定する `NMJSON_TRY_TO_BE_RIGID` は、Unicodeのカテゴリ表が手書きで不完全です。これを定義したのは誰だぁっ！！
* 重複したキーはそのまま受け入れます（`nmjson_object_get_*` は先に見つかった方を返します）
* `INT64_MAX` を超える整数はビットそのまま `int64_t` に格納されます。uint64の範囲も超えるとdoubleになります
* `nmjson_token_fout*` はデバッグ用です
* ほかにダメな点あったらissue送ってねマジで。

## 移植性

* 下記の環境でのビルドを確認
    * Cygwin
    * 通常のLinux (Intel CPU)
    * bitbake

## TODO

* トークンの追加（不定長メッセージで、受信バッファの入れ替えと対になるもの）
* JSONPathの記法でJSONを組み立てるビルダー（`src/builder/NOTE.txt`）
  →決まった形の通信にはwriterで足りるため、現在は保留

## あんちょこ

```
$ autoreconf -vfi
```

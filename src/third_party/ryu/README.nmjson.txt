Ryu (https://github.com/ulfjack/ryu)
  commit : 4c0618b0e44f7ef027ebae05d2cc7812048f7c8f
  license: Apache-2.0 または Boost-1.0 の選択式。libnmjsonは Boost Software License 1.0 を採用する。
           (LICENSE-Boost 参照。LICENSE-Apache2 は原本同梱のため保持)

取り込み内容:
  common.h / d2s_intrinsics.h / d2s_small_table.h : 無改変
  ryu/d2s.c : d2d() と d2d_small_int() のみを src/numfmt/nmjson_numfmt_f64_.c へ抜粋。
              to_chars()/d2s()(malloc使用)は取り込まない。出力整形はJSON向けに自前実装。

ビルド設定:
  RYU_OPTIMIZE_SIZE を nmjson_numfmt_f64_.c 内で定義（小テーブル版。約1KB未満）
  __SIZEOF_INT128__ が無い環境(32bit gcc等)では upstream の64bit演算フォールバックが自動で使われる。

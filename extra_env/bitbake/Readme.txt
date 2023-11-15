bitbake環境に入れるためのファイル

1: bbレシピファイルのファイル名のうち、%の部分を任意のものに差し替える
2: bbレシピファイルと同じディレクトリに、バージョン名を冠したディレクトリを作る
　 例えば"0.7.1"

3: リリースされたアーカイブを2のディレクトリに入れる

------------------------------------
The file to install into bitbake environment.

1: Rename "%" part of the .bb recipe filename into arbitrary version.
2: Make directory named the version decided in [1] into the same directory of .bb recipe file.
3: Put the released archive into the [2] directory.

------------------------------------
recipes-directory/
 |
 +-libnmjson/
   |
   +-libnmjson_x.x.x.bb
   |
   +-x.x.x/
     |
     +-libnmjson-0.7.1.tar.gz
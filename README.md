C++11 Markdown Subset to HTML converter
=======================================

This is a C++11 implementation of John Gruber's [Markdown][1].
it is not compliant with the reference implementation.

 [1]: http://daringfireball.net/projects/markdown/

BUILD
====

Current Makefile requires LLVM clang++ compiler.
Tested under the GNU libstdc++ 3 and LLVM libc++
on the C++ API GNU libstdc++. To decode or encode
UTF-8 octets for escaping URI, this requires
C.UTF-8 locale on the platform.

    $ make
    $ pushd mdtest/1.1
    $ make test
    $ popd
    $ ./mkdown < your_markdown_file

mkdown commands is a filter from stdin to stdout
without command line options.

EXPERIMENTAL
============

This provides you to markdown simplified `<ruby>` elements. For example:

    support [FURIGANA]^(rubi) elements.

Will produce:

    <p>support <ruby>FURIGANA<rp>(</rp><rt>rubi</rt><rp>)</rp></ruby> elements.</p>

Since this supports single `<rt>` element in `<ruby>` one,
you must write explicit `<ruby>` elements for cascading `<rt>` cases.

    not support <ruby>KAN<rt>kan</rt>JI<rt>ji</rt></ruby> elements.

This `<ruby>` extension is in experimental.


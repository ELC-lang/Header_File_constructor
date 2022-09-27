# Header_File_constructor

Intelligent pre-processing of headers to generate single file headers without system header content

## usage

```text
Header_File_constructor [options] in_file out_file
Options:
  -f, --full
    Even under folder handling each file is guaranteed to be included individually without error
  -h, --help
    Display help
  -r, --relocate
    Relocate the input file path in "#line" to the specified path
if in_file is a directory, out_file must be a directory or not exist,
and all superficial files in in_file will be processed.
```

## example

[online_cpp_header_files](https://github.com/ELC-lang/online_cpp_header_files/tree/master/files) form [`elc`](https://github.com/ELC-lang/ELC/blob/master/parts/header_file/files/elc)

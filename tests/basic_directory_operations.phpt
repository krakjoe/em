--TEST--
VFS: Directory creation and detection
--FILE--
<?php
file_put_contents("vfs://dir/file.txt", "content");
var_dump(is_dir("vfs://dir"));
var_dump(is_file("vfs://dir/file.txt"));
?>
--EXPECT--
bool(true)
bool(true)


--TEST--
VFS: File deletion with unlink
--FILE--
<?php
file_put_contents("/delete_me.txt", "temporary");
var_dump(file_exists("/delete_me.txt"));
var_dump(file_exists("vfs://delete_me.txt"));
unlink("/delete_me.txt");
var_dump(file_exists("/delete_me.txt"));
var_dump(file_exists("vfs://delete_me.txt"));
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(false)

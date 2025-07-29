
--TEST--
VFS: File deletion with unlink
--FILE--
<?php
file_put_contents("vfs://delete_me.txt", "temporary");
var_dump(file_exists("vfs://delete_me.txt"));
unlink("vfs://delete_me.txt");
var_dump(file_exists("vfs://delete_me.txt"));
?>
--EXPECT--
bool(true)
bool(false)

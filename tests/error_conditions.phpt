--TEST--
VFS: Error handling for non-existent files
--FILE--
<?php
$result = @file_get_contents("/nonexistent.txt");
var_dump($result === false);
$stat = @stat("/nonexistent.txt");
var_dump($stat === false);
?>
--EXPECT--
bool(true)
bool(true)

--TEST--
VFS: scandir functionality
--FILE--
<?php
file_put_contents("/file1.txt", "content1");
file_put_contents("/file2.txt", "content2");
$entries = scandir("/");
sort($entries);
foreach ($entries as $entry) {
    if ($entry !== '.' && $entry !== '..') {
        echo $entry . "\n";
    }
}
?>
--EXPECT--
file1.txt
file2.txt

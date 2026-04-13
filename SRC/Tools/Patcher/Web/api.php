<?php
	$key = "DW9B0yN";
	date_default_timezone_set('Europe/Lisbon');
	try {
		if(!isset($_GET['key']) || (isset($_GET['key']) && $_GET['key'] != $key))
			die('Invalid api-key.');

		if(!isset($_GET['opt']))
			die('Invalid option argument.');
	
		$opt = $_GET['opt'];

		switch($opt)
		{
			case 'creator':
			{
				function RecursiveDirectoryScan($Directory, $File) {
					$RII = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($Directory),
					RecursiveIteratorIterator::SELF_FIRST);
					$RIILns = array();
					foreach ($RII as $FN => $F)
						if (!$F->isDir())
							array_push($RIILns, sprintf('%s|%s|%d', str_ireplace('client/', '', $FN), md5_file($FN), $F->getSize())); 
							$SC = implode(PHP_EOL, $RIILns);
							$SC = str_replace("/","\\",$SC);
							file_put_contents($File, $SC);
				}
				RecursiveDirectoryScan('client', 'Patchlist.txt');
				echo 'done';
			}
			break;
			case 'info':
			{
				
				function bytes_umrechnen($size) {
					if ($size >= 1073741824) {
						return round(($size / 1073741824), 2) . " GB";
					}
					else if($size >= 1048576) {
						return round(($size / 1048576), 2) . " MB";
					}
					else if($size >= 1024) {
						return round(($size / 1024), 2) . " KB";
					}
					else {
						return $size . " Byte";
					}
				} 
				
				$file_name = 'Patchlist.txt';
				$file_count = 0;
				$folder_size = 0;
				$patchlist_modifiedmtime = date('d.m.Y H:i:s', filemtime($file_name));
				if ($file = fopen($file_name, "r")) {
					while(!feof($file)) {
						$line = fgets($file);
						$vals = explode('|', $line);
						$folder_size += (int) $vals[2];
						$file_count++;
					}
					fclose($file);
				}
				echo sprintf("%s|%s|%s", $file_count, bytes_umrechnen($folder_size), $patchlist_modifiedmtime);
			}
			break;
			case 'update':
			{
				$fp = fopen('Patchlist.txt', 'w');
				fwrite($fp, 'update');
				fclose($fp);
				echo sprintf("done");
			}
			break;
		}
	} catch (Exception $e) {
		echo 'Caught exception: ',  $e->getMessage(), "\n";
	}
?>
<!DOCTYPE html>
<html lang="en">
  <head>
  <!-- Latest compiled and minified CSS -->
	<link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css" integrity="sha384-BVYiiSIFeK1dGmJRAkycuHAHRg32OmUcww7on3RYdg4Va+PmSTsz/K68vbdEjh4u" crossorigin="anonymous">

	<!-- Optional theme -->
	<link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap-theme.min.css" integrity="sha384-rHyoN1iRsVXV4nD0JutlnGaslCJuC7uwjduW9SVrLvRYooPp2bWYgmgJQIXwl/Sp" crossorigin="anonymous">

<!-- Latest compiled and minified JavaScript -->
<script src="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js" integrity="sha384-Tc5IQib027qvyjSMfHjOMaLkfuWVxZxUPnCJA7l2mCWNIpG9mGCD8wGNIcPD7Txa" crossorigin="anonymous"></script>
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <!-- The above 3 meta tags *must* come first in the head; any other head content must come *after* these tags -->
    <title>eBee</title>

    <!-- Bootstrap -->
    <link href="css/bootstrap.min.css" rel="stylesheet">

  </head>
  <body>
    <h1>Display data from MySql database <i>ebee</i></h1>
	
<?php 

	$db_host = '127.0.0.1';
	$db_user = 'username';
	$db_pass = 'password';
	$db_name = 'ebee';
	
	$conn = mysqli_connect($db_host, $db_user, $db_pass, $db_name);
	if(!$conn)
	{
		die ('Failed to connect to mysql database' . mysqli_connect_error);
	}
	
	
	$sql = "SELECT * FROM beetable";
	$query = mysqli_query($conn, $sql);
	
	if(!$query)
	{
		die ('Error found' . mysqli_error($conn));
	}
	
$conn->close();
	
	echo "
		<table class = 'table'>
		<tr>
		<th>Weight</th>
		<th>Temperature</th>
		<th>Humidity</th>
		<th>Date</th>
		</tr>";
		
		while($row = mysqli_fetch_array($query))
		{
			echo ' <tr>
			<td>'.$row['weight'].'</td>
			<td>'.$row['temp'].'</td>
			<td>'.$row['hum'].'</td>
			<td>'.$row['date'].'</td>
			</tr>';
		}
		
		echo "</table>";

?>
<!-- jQuery (necessary for Bootstrap's JavaScript plugins) -->
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js"></script>
    <!-- Include all compiled plugins (below), or include individual files as needed -->
    <script src="js/bootstrap.min.js"></script>
  </body>
</html>
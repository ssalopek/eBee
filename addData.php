<html>
 <head>
 <?php
   // PHP program is placed in the head so that it is executed
   // when a HTTP HEAD request is received. This avoids a lot of useless
   // data that is transferred with the body in response to a HTTP GET request.

   $servername = "127.0.0.1";
   $username = "username";
   $password = "password";
   $dbname = "ebee";

   // Create connection
   $conn = new mysqli($servername, $username, $password, $dbname);
   // Check connection
   if ($conn->connect_error) 
   {
    echo '<p>Connection failed</p>';
    die("Connection failed: " . $conn->connect_error);
   } 

   // Insert new record into the database
   $sql = "INSERT INTO beetable (weight, temp, hum) VALUES ('". $_GET["weight"] .", ". $_GET["temp"] .", ". $_GET["hum"] ."')";
   $result = $conn->query($sql);

   // To prevent overload on the database, delete records older than 31 days
   $sql = "DELETE FROM beeTable WHERE Time < DATE_SUB(NOW(), INTERVAL 31 DAY);";
   $result = $conn->query($sql);

   $conn->close();
   ?> 
 </head>
 <body>
 </body>
</html>


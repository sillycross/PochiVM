#!/bin/bash
sudo mv /var/lib/mysql-files/customer.txt customer.txt
sudo chown $USER:$USER customer.txt
sudo mv /var/lib/mysql-files/lineitem.txt lineitem.txt
sudo chown $USER:$USER lineitem.txt
sudo mv /var/lib/mysql-files/nation.txt nation.txt
sudo chown $USER:$USER nation.txt
sudo mv /var/lib/mysql-files/orders.txt orders.txt
sudo chown $USER:$USER orders.txt
sudo mv /var/lib/mysql-files/part.txt part.txt
sudo chown $USER:$USER part.txt
sudo mv /var/lib/mysql-files/partsupp.txt partsupp.txt
sudo chown $USER:$USER partsupp.txt
sudo mv /var/lib/mysql-files/region.txt region.txt
sudo chown $USER:$USER region.txt
sudo mv /var/lib/mysql-files/supplier.txt supplier.txt
sudo chown $USER:$USER supplier.txt


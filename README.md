# keralavision-cli
A Command Line Utility to display information about your Kerala Vision Broad band by scraping the webpage.

Written in C (C99) without using any html parsing libraries.

## Dependency
libcurl

## Insatallation
1. Clone the project

         git clone https://github.com/anoop142/keralavision-cli

2. Build it

         make

3. Run it!
    
         ./kv-cli

## Usage
        kv-cli [-h]  [-u username] mode

    mode : 
        mode is one of the following

        all[default] 
            Prints all the basic information

        plan
            Prints plan name

        expiry
            Prints the date plan expires
        
        mac
            Prints the mac address of the  registered router

        ip
            Prints the ip address

        total_data
            Priints total used data
        
        session
            Prints data used in current session

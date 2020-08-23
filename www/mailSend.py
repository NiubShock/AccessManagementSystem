#!/usr/bin/env python
import smtplib
import sys
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText


def getReceivers(_CA_identity):
    receiver_time = []
    file = open(path, 'r+')
    keepIt = False
    for line in file:
        try:
            stringPart = line.split('|')

            castring = str(stringPart[4]).replace('\n', '')

            #check if is the infected user
            if castring == _CA_identity:
                #create a list with all the hours hh:mm:ss
                _temp = stringPart[2].split(':')
                date.append(int(_temp[0]) * 60 + int(_temp[1]))

            #for each line get the receiver address as well as the entering time
            receiver.append(stringPart[3])
            _temp = stringPart[2].split(':')
            receiver_time.append(int(_temp[0]) * 60 + int(_temp[1]))

        except:
            print("ERROR")


    i = 0
    #check every possible user
    for _rec in receiver:
        #check every possible hours
        for element in date:
            keepIt = False
            if int(receiver_time[i]) - int(element) < 120:
                keepIt = True
                break

        if keepIt == False:
            receiver.remove(_rec)

        i = i + 1

    #what is left is the correct list of user -> inside receiver



def sendEmail(sender, password, email_list):
    _sender = sender
    _receiver = email_list
    _password = password

    _message = MIMEMultipart()
    _message['Subject'] = 'Covid19 Prevention'
    _message['From'] = sender

    _mex_text = 'Dear user,\nyou get in contact with a covid19 infected subject.\nPlease call your doctor and take care.\nThis email is automatically generated.\n\nThanks\nPlease do not answer to this email'
    

    _mex_body = MIMEText(_mex_text, 'plain')
    _message.attach(_mex_body)

    #create an smtp object
    _smtpObj = smtplib.SMTP('smtp.gmail.com', 587)
    #start tls
    _smtpObj.ehlo()
    _smtpObj.starttls()
    _smtpObj.ehlo()
    _smtpObj.login(_sender,_password)
    
    for _email in _receiver:
        _message['To'] = str(_email)
        _smtpObj.sendmail(_sender, str(_email), _message.as_string())
    
    _smtpObj.quit()
    print("Done")





sender = 'yunproject247@gmail.com'
password = 'arduino_remote'
path = "/mnt/sda1/arduino/www/Project/www/List"
email = []
date = []
receiver = []

getReceivers(sys.argv[1])
receiver = list(set(receiver))

sendEmail(sender, password, receiver)   










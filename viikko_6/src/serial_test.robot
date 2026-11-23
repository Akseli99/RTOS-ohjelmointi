*** Settings ***
Library   String
Library   SerialLibrary

*** Variables ***
${com}      COM14
${baud}     115200
${board}    nRF

*** Test Cases ***
Connect Serial
	Log To Console  Connecting to ${board}
	Add Port  ${com}  baudrate=${baud}  encoding=ascii
	Port Should Be Open  ${com}
	Reset Input Buffer
	Reset Output Buffer

Test Valid Time String
    [Documentation]  Testataan oikeaa aikaa: 000120 = 80 sekuntia
	Reset Input Buffer
    ${seq}=   Set Variable   000120\n
    Write Data   ${seq}   encoding=ascii 
    Log To Console   Send sequence 000120

    ${read} =   Read Until   terminator=58   encoding=ascii 
    Log To Console   Received ${read}
    
    ${clean_read}=   Remove String   ${read}   X
    ${clean_read}=   Strip String    ${clean_read}
    
    Should Be Equal As Integers   ${clean_read}    80
    Log To Console   Test passed: 000120 resulted in 80 seconds.

Test Invalid Time Seconds
    [Documentation]  Testataan virheellisiä sekunteja
	Reset Input Buffer
    ${seq}=   Set Variable   001067\n
    Write Data   ${seq}   encoding=ascii 
    Log To Console   Send sequence 001067

    ${read} =   Read Until   terminator=58   encoding=ascii 
    Log To Console   Received ${read}
    
    ${clean_read}=   Remove String   ${read}   X
    ${clean_read}=   Strip String    ${clean_read}
    
    Should Be Equal As Integers   ${clean_read}    -2
    Log To Console   Test passed: 001067 returned error code -2.

Test Invalid Time Length
    [Documentation]  Testataan liian lyhyttä merkkijonoa
	Reset Input Buffer
    ${seq}=   Set Variable   12345\n
    Write Data   ${seq}   encoding=ascii 
    Log To Console   Send sequence 12345

    ${read} =   Read Until   terminator=58   encoding=ascii 
    Log To Console   Received ${read}

	${clean_read}=   Remove String   ${read}   X
    ${clean_read}=   Strip String    ${clean_read}
    
    Should Be Equal As Integers   ${clean_read}    -1
    Log To Console   Test passed: 12345 returned error code -1.

Disconnect Serial
    Log To Console  Disconnecting ${board}
    [TearDown]  Delete Port  ${com}
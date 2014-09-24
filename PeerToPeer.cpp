#include "PeerToPeer.h"
#include "KeyManager.h"
#include "base64.h"
#include "PeerIO.cpp"

int PeerToPeer::StartServer(const int MAX_CLIENTS, bool SendPublic, string SavePublic)
{
	//		**-SERVER-**
	if((Serv = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)		//assign Serv to a file descriptor (socket) that uses IP addresses, TCP
	{
		close(Serv);
		return -1;
	}
	
	memset(&socketInfo, 0, sizeof(socketInfo));				//Clear data inside socketInfo to be filled with server stuff
	socketInfo.sin_family = AF_INET;					//Use IP addresses
	socketInfo.sin_addr.s_addr = htonl(INADDR_ANY);				//Allow connection from anybody
	socketInfo.sin_port = htons(Port);					//Use port Port
	
	int optval = 1;
	setsockopt(Serv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);	//Remove Bind already used error
	if(bind(Serv, (struct sockaddr*)&socketInfo, sizeof(socketInfo)) < 0)	//Bind socketInfo to Serv
	{
		close(Serv);
		perror("Bind");
		return -2;
	}
	listen(Serv, MAX_CLIENTS);			//Listen for connections on Serv
	
	//		**-CLIENT-**
	if(ClntIP.empty())				//If we didn't set the client ip as an argument
	{
		while(!IsIP(ClntIP))			//Keep going until we enter a real ip
		{
			if(!ClntIP.empty())
				cout << "That is not a properly formated IPv4 address and will not be used\n";
			cout  << "Client's IP: ";
			getline(cin, ClntIP);
		}
	}
	
	Client = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);			//assign Client to a file descriptor (socket) that uses IP addresses, TCP
	memset(&socketInfo, 0, sizeof(socketInfo));				//Clear socketInfo to be filled with client stuff
	socketInfo.sin_family = AF_INET;					//uses IP addresses
	socketInfo.sin_addr.s_addr = inet_addr(ClntIP.c_str());			//connects to the ip we specified
	socketInfo.sin_port = htons(Port);					//uses port Port
	
	//		**-FILE DESCRIPTORS-**
	FD_ZERO(&master);							//clear data in master
	FD_SET(Serv, &master);							//set master to check file descriptor Serv
	read_fds = master;							//the read_fds will check the same FDs as master
	
	MySocks = new int[MAX_CLIENTS + 1];					//MySocks is a new array of sockets (ints) as long the max connections + 1
	MySocks[0] = Serv;							//first socket is the server FD
	for(unsigned int i = 1; i < MAX_CLIENTS + 1; i++)			//assign all the empty ones to -1 (so we know they haven't been assigned a socket)
		MySocks[i] = -1;
	timeval zero = {0, 50};							//called zero for legacy reasons... assign timeval 50 milliseconds
	fdmax = Serv;								//fdmax is the highest file descriptor to check (because they are just ints)
	
	//Progress checks
	SentStuff = 0;
	GConnected = false;							//GConnected allows us to tell if we have set all the initial values, but haven't begun the chat
	ConnectedClnt = false;
	ConnectedSrvr = false;
	ContinueLoop = true;
	
	nonblock(true, false);							//nonblocking input, disable echo
	while(ContinueLoop)
	{
		if(!GConnected && ConnectedClnt && ConnectedSrvr && SentStuff == 3)	//All values have been sent, then set, but we haven't begun! Start already!
		{
			GConnected = true;
			cout << "\r          \r";
			currntLength = 0;
			CurPos = 0;
			for(int j = 0; j < 512; j++)
				OrigText[j] = '\0';

			cout << "Message: ";
		}
		
		read_fds = master;						//assign read_fds back to the unchanged master
		if(select(fdmax+1, &read_fds, NULL, NULL, &zero) == -1)		//Check for stuff to read on sockets, up to fdmax+1.. stop check after timeval zero (50ms)
		{
			cout << "\r";
			for(int j = 0; j < currntLength + 9; j++)
				cout << " ";
			cout << "\r";
			
			perror("Select");
			return -3;
		}
		for(unsigned int i = 0; i < MAX_CLIENTS + 1; i++)		//Look through all sockets
		{
			if(MySocks[i] == -1)					//if MySocks[i] == -1 then go just continue the for loop, this part of the array hasn't been assigned a socket
				continue;
			if(FD_ISSET(MySocks[i], &read_fds))			//check read_fds to see if there is unread data in MySocks[i]
			{
				if(i == 0)		//if i = 0, then based on line 52, we know that we are looking at data on the Serv socket... This means a new connection!!
				{
					if((newSocket = accept(Serv, NULL, NULL)) < 0)		//assign socket newSocket to the person we are accepting on Serv
					{
						close(Serv);				//unless it errors
						perror("Accept");
						return -4;
					}
					ConnectedSrvr = true;		//Passed All Tests, We Can Safely Say We Connected
					
					FD_SET(newSocket, &master); // add the newSocket FD to master set
					for(unsigned int j = 1; j < MAX_CLIENTS + 1; j++)	//assign an unassigned MySocks to newSocket
					{
						if(MySocks[j] == -1) 	//Not in use
						{
							MySocks[j] = newSocket;
							if(newSocket > fdmax)		//if the new file descriptor is greater than fdmax..
								fdmax = newSocket;	//change fdmax to newSocket
							break;
						}
					}
					
					if(ClientMod == 0)		//Check if we haven't already assigned the client's public key through an arg.
					{
						char TempVA[1060] = {'\0'};
						string TempVS;
						recv(newSocket, TempVA, 1060, 0);
						TempVS = TempVA;
						
						try
						{
							Import64(TempVS.substr(0, TempVS.find("|", 1)), ClientMod);	//Modulus in Base64 in first half
							//cout << "CM: " << Export64(ClientMod) << "\n\n";
						}
						catch(int e)
						{
							cout << "The received modulus is bad\n";
							return -1;
						}
						
						try
						{
							Import64(TempVS.substr(TempVS.find("|", 1)+1), ClientE);	//Encryption key in Base64 in second half
							//cout << "CE: " << Export64(ClientE) << "\n\n";
						}
						catch(int e)
						{
							cout << "The received RSA encryption key is bad\n";
							return -1;
						}
						if(!SavePublic.empty())		//If we set the string for where to save their public key...
							MakePublicKey(SavePublic, ClientMod, ClientE);		//SAVE THEIR PUBLIC KEY!
					}
				}
				else		//Data is on a new socket
				{
					char buf[RECV_SIZE] = {'\0'};	//RECV_SIZE is the max possible incoming data (2048 byte file part with 24 byte iv and leading byte)
					for(unsigned int j = 0; j < RECV_SIZE; j++)
						buf[j] = '\0';
					
					if((nbytes = recv(MySocks[i], buf, RECV_SIZE, 0)) <= 0)		//handle data from a client
					{
						// got error or connection closed by client
						if(nbytes == 0)
						{
							// connection closed
							cout << "\r";
							for(int j = 0; j < currntLength + 9; j++)
								cout << " ";
							cout << "\r";
							cout <<"Server: socket " << MySocks[i] << " hung up\n";
							return 0;
						}
						else
						{
							cout << "\r         \r";
							perror("Recv");
						}
						close(MySocks[i]); // bye!
						MySocks[i] = -1;
						FD_CLR(MySocks[i], &master); // remove from master set
						ContinueLoop = false;
					}
					else if(SentStuff == 2)		//if SentStuff == 2, then we still need the symmetric key
					{
						string ClntKey = buf;
						mpz_class TempKey;
						try
						{
							Import64(ClntKey, TempKey);
						}
						catch(int e)
						{
							cout << "The received symmetric key is bad\n";
						}
						SymKey += MyRSA.BigDecrypt(MyMod, MyD, TempKey);		//They sent their sym key with our public key. Decrypt it!
						
						mpz_class LargestAllowed = 0;
						mpz_class One = 1;
						mpz_mul_2exp(LargestAllowed.get_mpz_t(), One.get_mpz_t(), 128);		//Largest allowed sym key is equal to (1 * 2^128)
						SymKey %= LargestAllowed;		//Modulus by largest 128 bit value ensures within range after adding keys!
						SentStuff = 3;
					}
					else
					{
						string Msg = "";							//lead byte for data id | varying extension info		| main data
																	//-----------------------------------------------------------------------------------------------------
																	//0 = msg        	 	| IV64_LEN chars for IV			| 512 message chars (or less)
																	//1 = file request	 	| X chars for file length		| Y chars for file loc.
																	//2 = request answer 	| 1 char for answer				|
																	//3 = file piece	 	| IV64_LEN chars for IV			| FILE_PIECE_LEN bytes of file piece (or less)
						
						for(unsigned int i = 0; i < nbytes; i++)	//If we do a simple assign, the string will stop reading at a null terminator ('\0')
							Msg.push_back(buf[i]);					//so manually push back all values in array buf...
						
						if(Msg[0] == 0)
						{
							try
							{
								Import64(Msg.substr(1, IV64_LEN), PeerIV);
							}
							catch(int e)
							{
								cout << "The received IV is bad\n";
							}

							Msg = Msg.substr(IV64_LEN+1);
							DropLine(Msg);
							if(Sending == 0)
								cout << "\nMessage: " << OrigText;							//Print what we already had typed (creates appearance of dropping current line)
							else if(Sending == 1)
								cout << "\nFile Location: " << OrigText;
							for(int setCur = 0; setCur < currntLength - CurPos; setCur++)	//set cursor position to what it previously was (for when arrow keys are handled)
								cout << "\b";
						}
						else if(Msg[0] == 1)
						{
							Sending = -1;		//Receive file mode
							Import64(Msg.substr(1, IV64_LEN), PeerIV);
							string PlainText;
							try
							{
								PlainText = MyAES.Decrypt(SymKey, Msg.substr(IV64_LEN+1), PeerIV);
							}
							catch(string e)
							{
								cout << e << endl;
							}
							FileLength = atoi(PlainText.substr(0, PlainText.find("X", 1)).c_str());
							FileLoc = PlainText.substr(PlainText.find("X", 1)+1);
							cout << "\rSave " << FileLoc << ", " << FileLength << " bytes<y/N>";
							char c = getch();
							cout << c;
							if(c == 'y' || c == 'Y')
							{
								c = 'y';
								BytesRead = 0;
							}
							else
							{
								c = 'n';
								Sending = 0;
							}
							string Accept = "xx";
							Accept[0] = 2;
							Accept[1] = c;
							send(Client, Accept.c_str(), Accept.length(), 0);
							cout << "\nMessage: " << OrigText;
						}
						else if(Msg[0] == 2)
						{
							if(Msg[1] == 'y')
							{
								Sending = 3;
								FilePos = 0;
							}
							else
							{
								Sending = 0;
								cout << "\r";
								for(int i = 0; i < currntLength + 15; i++)
									cout << " ";
								cout << "\rPeer rejected file. The transfer was cancelled.";
							}
							cout << "\nMessage: ";
							for(int i = 0; i < 512; i++)
								OrigText[i] = '\0';
							CurPos = 0;
							currntLength = 0;
						}
						else if(Msg[0] == 3)
						{
							try
							{
								Import64(Msg.substr(1, IV64_LEN), FileIV);
							}
							catch(int e)
							{
								cout << "Bad IV value when receiving file\n";
							}
							ReceiveFile(Msg.substr(IV64_LEN+1));
						}
					}
				}
			}//End FD_ISSET
		}//End For Loop for sockets
		if(kbhit())		//Check for keypress
		{
			if(GConnected && Sending != 2)		//So nothing happens until we are ready...
				ParseInput();
			else
				getch();		//And keypresses before hand arent read when we are.
		}
		if(Sending == 3)
			SendFilePt2();
		if(!ConnectedClnt)		//Not conected yet?!?
		{
			TryConnect(SendPublic);		//Lets try to change that
		}
		if(SentStuff == 1 && ClientMod != 0 && ClientE != 0)		//We have established a connection and we have their keys!
		{
			string MyValues = Export64(MyRSA.BigEncrypt(ClientMod, ClientE, SymKey));	//Encrypt The Symmetric Key With Their Public Key, base 64
			
			//Send The Encrypted Symmetric Key
			if(send(Client, MyValues.c_str(), MyValues.length(), 0) < 0)
			{
				perror("Connect failure");
				return -5;
			}
			SentStuff = 2;			//We have given them our symmetric key
		}
		fflush(stdout);		//Not always does cout print immediately, this forces it.
	}//End While Loop
	cout << "\n";
	close(Serv);
	close(Client);
	return 0;
}

void PeerToPeer::TryConnect(bool SendPublic)
{
	if(connect(Client, (struct sockaddr*)&socketInfo, sizeof(socketInfo)) >= 0) 	//attempt to connect using socketInfo with client values
	{
		fprintf(stderr, "Connected!\n");
		if(SendPublic)
		{
			string TempValues = "";
			string MyValues = "";

			TempValues = Export64(MyMod);		//Base64 will save digits
			MyValues = TempValues + "|";		//Pipe char to seperate keys

			TempValues = Export64(MyE);
			MyValues += TempValues;				//MyValues is equal to the string for the modulus + string for exp concatenated

			//Send My Public Key And My Modulus Because We Started The Connection
			if(send(Client, MyValues.c_str(), MyValues.length(), 0) < 0)
			{
				perror("Connect failure");
				return;
			}
		}
		SentStuff = 1;			//We have sent our keys
			
		ConnectedClnt = true;
		fprintf(stderr, "Waiting...");
	}
	return;
}

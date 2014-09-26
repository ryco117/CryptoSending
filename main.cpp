#include <iostream>
#include <string>
#ifndef ANDROID
#include <ifaddrs.h>
#endif

#define IV64_LEN 24
#define FILE_PIECE_LEN 2048
#define RECV_SIZE (1 + IV64_LEN + FILE_PIECE_LEN + 16)

#include "PeerToPeer.cpp"
#include "myconio.h"
#include "KeyManager.h"

using namespace std;

void GMPSeed(gmp_randclass& rng);
void PrintIP();
string GetPassword();

string HelpString = \
"Secure chat program written by Ryan Andersen\n\
Contact at ryco117@gmail.com\n\n\
Arguments List\n\n\
Toggles:\n\
-p,\t--print\t\t\tprint all generated encryption values\n\
-m,\t--manual\t\tWARNING! this stops auto-assigning random RSA key values and is pretty much strictly for debugging\n\
-dp,\t--disable-public\tdon't send our public key at connection. WARNING! peer must use -lp and have our public key\n\
-h,\t--help\t\t\tprint this dialogue\n\n\
String Inputs:\n\
-ip,\t--ip-address\t\tspecify the ip address to attempt to connect to\n\
-o,\t--output\t\tsave the rsa keys generated to files which can be reused\n\
-sp,\t--save-public\t\tsave the peer's public key to a specified file\n\
-lk,\t--load-keys\t\tspecify the files to load rsa keys from (public and private) that we will use\n\
-lp,\t--load-public\t\tspecify the file to load rsa public key from that the peer has the private key to. WARNING! peer must use -dp\n\n\
Integer Inputs:\n\
-P, --ports\t\t\tthe port number to open and connect to\n\n\
Input Argument Examples:\n\
-ip 192.168.1.70\t\twill attempt to connect to 192.168.1.70\n\
-o newKeys\t\t\twill produce newKeys.pub and newKeys.priv\n\
-sp peerKey.pub\t\t\twill create the file peerKey.pub with the peer's rsa public key\n\
-lk Keys\t\t\twill load the rsa values from the files Keys.pub and Keys.priv\n\
-lp PeerKey.pub\t\t\twill load the peer's public key from PeerKey.pub\n\
-P 4321\t\t\t\twill open port number 4321 for this session, and will connect to the same number\n\n";

int main(int argc, char* argv[])
{
	//This will be used for all randomness for the rest of the execution... seed well
	gmp_randclass rng(gmp_randinit_default);		//Define a gmp_randclass, initialize with default value
	GMPSeed(rng);						//Pass randclass to function to seed it for more random values
	
	PeerToPeer MyPTP;
	MyPTP.Port = 5001;
	MyPTP.ClientMod = 0;
	MyPTP.ClientE = 0;
	MyPTP.Sending = 0;
	
	RSA NewRSA;
	AES Cipher;
	mpz_class SymmetricKey = rng.get_z_bits(256);		//Create a 128 bit long random value as our key
	mpz_class Keys[2] = {0};
	mpz_class Mod = 0;
	
	bool PrintVals = false;
	bool ForceRand = true;
	bool SendPublic = true;
	string SavePublic = "";
	string OutputFiles = "";
	
	for(unsigned int i = 1; i < argc; i++)		//What arguments were we provided with? How should we handle them
	{
		string Arg = string(argv[i]);
		if(Arg == "-p" || Arg == "--print")
			PrintVals = true;
		else if(Arg == "-m" || Arg == "--manual")
			ForceRand = false;
		else if((Arg == "-ip" || Arg == "--ip-address") && i+1 < argc)
		{
			MyPTP.ClntIP = argv[i+1];
			if(!IsIP(MyPTP.ClntIP))
			{
				cout << MyPTP.ClntIP << " is not a properly formated IPv4 address and will not be used\n";
				MyPTP.ClntIP = "";
			}
			i++;
		}
		else if((Arg == "-o" || Arg == "--output") && i+1 < argc)	//Write two keys files
		{
			OutputFiles = argv[i+1];
			i++;
		}
		else if((Arg == "-lk" || Arg == "--load-keys") && i+1 < argc)	//load the public and private keys we will use
		{
			string PubKeyName = string(argv[i+1]) + ".pub";
			string PrivKeyName = string(argv[i+1]) + ".priv";
			cout << "Private Key Password: ";
			fflush(stdout);
			
			string Passwd = GetPassword();
			if(!LoadPrivateKey(PrivKeyName, Keys[1], &Passwd))
			{
				Keys[1] = 0;
				return -1;
			}
			if(!LoadPublicKey(PubKeyName, Mod, Keys[0]))
			{
				Mod = 0;
				Keys[0] = 0;
				Keys[1] = 0;
				return -1;
			}
			i++;
		}
		else if((Arg == "-lp" || Arg == "--load-public") && i+1 < argc)	//load the public key that the peer can decrypt
		{
			if(!LoadPublicKey(argv[i+1], MyPTP.ClientMod, MyPTP.ClientE))
			{
				MyPTP.ClientMod = 0;
				MyPTP.ClientE = 0;
			}
			i++;
		}
		else if((Arg == "-P" || Arg == "--port") && i+1 < argc)
		{
			MyPTP.Port = atoi(argv[i+1]);
			if(MyPTP.Port <= 0)
			{
				cout << "Bad port number. Using default 5001\n";
				MyPTP.Port = 5001;
			}
			i++;
		}
		else if(Arg == "-dp" || Arg == "--disable-public")		//WARNIG only if peer already has public your public key and uses -lp
			SendPublic = false;
		else if((Arg == "-sp" || Arg == "--save-public") && i+1 < argc)
		{
			SavePublic = argv[i+1];
			i++;
		}
		else if(Arg == "-h" || Arg == "--help")
		{
			cout << HelpString;
			return 0;
		}
		else			//What the hell were they trying to do?
			cout << "warning: didn't understand " << Arg << endl;
	}
	#ifndef ANDROID
	PrintIP();
	#endif

	if(PrintVals)
		cout <<"Symmetric Key: 0x" << SymmetricKey.get_str(16) << "\n\n";
	
	GMPSeed(rng);		//Reseed for more secure random goodness
	if(Mod == 0)		//If one is not set, they all must be set (And if one has a set value, they all must)
		NewRSA.KeyGenerator(Keys, Mod, rng, ForceRand, PrintVals);
		
	if(!OutputFiles.empty())		//So, we want to output the generated keys
	{
		string PubKeyName = OutputFiles + ".pub";
		string PrivKeyName = OutputFiles + ".priv";

		while(true)
		{
			cout << "Private Key Password To Use: ";
			fflush(stdout);
			string Passwd1 = GetPassword();
			cout << "Retype Password: ";
			fflush(stdout);
			string Passwd2 = GetPassword();
			if(Passwd1 != Passwd2)		//Mistype
			{
				cout << "Passwords do not match. Do you want to try again<Y/n>: ";
				fflush(stdout);
				string Answer;
				getline(cin, Answer);
				if(Answer == "n" || Answer == "N")
				{
					cout << "Giving up on key creation\n\n";		//Because of a mistype? Pathetic...
					break;
				}
			}
			else
			{
				char SaltStr[16] = {0};
				int n = 0;

				mpz_class Salt = rng.get_z_bits(128);
				mpz_export(SaltStr, (size_t*)&n, 1, 1, 0, 0, Salt.get_mpz_t());
				mpz_class TempIV = rng.get_z_bits(128);
				if(PrintVals)
				{
					cout << "Salt: " << Export64(Salt) << endl;
					cout << "IV: " << TempIV.get_str(16) << endl;
				}

				MakePrivateKey(PrivKeyName, Keys[1], &Passwd1, SaltStr, TempIV);
				MakePublicKey(PubKeyName, Mod, Keys[0]);
				break;
			}
		}
	}

	cout << "All necessary Encryption values are filled\n\n";
	MyPTP.MyMod = Mod;
	MyPTP.MyE = Keys[0];
	MyPTP.MyD = Keys[1];
	MyPTP.SymKey = SymmetricKey;
	GMPSeed(rng);
	MyPTP.RNG = &rng;

	if(MyPTP.StartServer(1, SendPublic, SavePublic) != 0)			//Jump to the loop to handle all incoming connections and data sending
	{
		nonblock(false, true);
		cout << "Finished cleaning, Press Enter To Exit...";
		cin.get();
		return 0;
	}
	
	nonblock(false, true);
	cout << "Finished cleaning, Press Enter To Exit...";
	cin.get();
	return 0;
}

void GMPSeed(gmp_randclass& rng)
{
	//Properly Seed rand()
	FILE* random;
	unsigned int seed;
	random = fopen ("/dev/urandom", "r");		//Unix provides it, why not use it
	if(random == NULL)
	{
		fprintf(stderr, "Cannot open /dev/urandom!\n"); 
		return;
	}
	for(int i = 0; i < 20; i++)
	{
		fread(&seed, sizeof(seed), 1, random);
		srand(seed); 		//seed the default random number generator
		rng.seed(seed);		//seed the GMP random number generator
	}
	fclose(random);
}

string GetPassword()
{
	string Passwd;
	nonblock(true, false);
	while(true)
	{
		if(kbhit())
		{
			unsigned char c = getch();
			if(c == '\n')
			{
				cout << "\n";
				nonblock(false, true);
				return Passwd;
			}
			else if(c == 127)	//Backspace
			{
				if(Passwd.length() > 0)
				{
					cout << "\b \b";
					Passwd = Passwd.substr(0, Passwd.length()-1);
				}
				else if(Passwd.length() == 1)
				{
					cout << "\b \b";
					Passwd.clear();
				}
			}
			else if((int)c >= 32 && (int)c <= 126)
			{
				Passwd += c;
				cout << "*";
			}
			else
			{
				getch();
				getch();
			}
			fflush(stdout);
		}
	}
}

#ifndef ANDROID
void PrintIP()
{
	struct ifaddrs *addrs, *tmp;
	getifaddrs(&addrs);
	tmp = addrs;


	while (tmp) 
	{
		if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET)
		{
			struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
			printf("%s: %s\n", tmp->ifa_name, inet_ntoa(pAddr->sin_addr));
		}

		tmp = tmp->ifa_next;
	}

	freeifaddrs(addrs);
	return;
}
#endif

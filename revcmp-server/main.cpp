#include "RakPeerInterface.h"
#include "BitStream.h"
#include "MessageIdentifiers.h"

#define SERVER_PORT 5040
#define SERVER_COUNT_ALLOWED_CLIENTS 50
#define SERVER_PASSWORD "Rumpelstiltskin"
#define COUNT_SOCKET_DESCRIPTOR 1

enum GameMessages {
	ID_PLAYER_COORDINATES = ID_USER_PACKET_ENUM + 1
};

struct PlayerInfo {
	int32_t id;
	float x, y, z;
};

struct PlayerInfo gPlayerInfo[2];

unsigned char GetPacketIdentifier(RakNet::Packet* p);
void UpdateNetwork(RakNet::RakPeerInterface* server);
void SendPlayerPositions(RakNet::RakPeerInterface* server);

int main()
{
	ZeroMemory(gPlayerInfo, sizeof(gPlayerInfo));

	RakNet::RakPeerInterface* server = RakNet::RakPeerInterface::GetInstance();
	server->SetIncomingPassword(SERVER_PASSWORD, (int)strlen(SERVER_PASSWORD));
	server->SetTimeoutTime(30000, RakNet::UNASSIGNED_SYSTEM_ADDRESS);

	RakNet::SocketDescriptor socketDescriptors[COUNT_SOCKET_DESCRIPTOR];
	socketDescriptors[0].port = SERVER_PORT;
	socketDescriptors[0].socketFamily = AF_INET;

	RakNet::StartupResult result = server->Startup(SERVER_COUNT_ALLOWED_CLIENTS, socketDescriptors, COUNT_SOCKET_DESCRIPTOR);
	server->SetMaximumIncomingConnections(SERVER_COUNT_ALLOWED_CLIENTS);

	if (result != RakNet::RAKNET_STARTED) {
		puts("Server failed to start. Terminating\n");
		exit(1);
	}

	printf("reVCMP server started on port: %d\n", SERVER_PORT);

	while (true) {
		Sleep(5);
		UpdateNetwork(server);
		SendPlayerPositions(server);
	}

	server->Shutdown(300);
	RakNet::RakPeerInterface::DestroyInstance(server);

	return 0;
}

unsigned char GetPacketIdentifier(RakNet::Packet* p)
{
	if (p == 0) {
		return 255;
	}

	if ((unsigned char)p->data[0] == ID_TIMESTAMP) {
		RakAssert(p->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
		return (unsigned char)p->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
	}

	return (unsigned char)p->data[0];
}


void UpdateNetwork(RakNet::RakPeerInterface* server)
{
	unsigned char packetIdentifier;

	RakNet::Packet* p;

	for (p = server->Receive(); p; server->DeallocatePacket(p), p = server->Receive())
	{
		// We got a packet, get the identifier with our handy function
		packetIdentifier = GetPacketIdentifier(p);

		// Check if this is a network message packet
		switch (packetIdentifier)
		{
		case ID_DISCONNECTION_NOTIFICATION:
			// Connection lost normally
			printf("ID_DISCONNECTION_NOTIFICATION from %s\n", p->systemAddress.ToString(true));;
			break;

		case ID_NEW_INCOMING_CONNECTION:
			// Somebody connected.  We have their IP now
			printf("ID_NEW_INCOMING_CONNECTION from %s with GUID %s\n", p->systemAddress.ToString(true), p->guid.ToString());

			printf("Remote internal IDs:\n");
			for (int index = 0; index < MAXIMUM_NUMBER_OF_INTERNAL_IDS; index++)
			{
				RakNet::SystemAddress internalId = server->GetInternalID(p->systemAddress, index);
				if (internalId != RakNet::UNASSIGNED_SYSTEM_ADDRESS)
				{
					printf("%i. %s\n", index + 1, internalId.ToString(true));
				}
			}

			break;

		case ID_INCOMPATIBLE_PROTOCOL_VERSION:
			printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
			break;

		case ID_CONNECTED_PING:
		case ID_UNCONNECTED_PING:
			printf("Ping from %s\n", p->systemAddress.ToString(true));
			break;

		case ID_CONNECTION_LOST:
			// Couldn't deliver a reliable packet - i.e. the other system was abnormally
			// terminated
			printf("ID_CONNECTION_LOST from %s\n", p->systemAddress.ToString(true));;
			break;

		case ID_PLAYER_COORDINATES:
		{
			int32_t plrId;
			float x, y, z;

			RakNet::BitStream myBitStream(p->data, p->length, false); // The false is for efficiency so we don't make a copy of the passed data
			myBitStream.IgnoreBytes(sizeof(RakNet::MessageID));
			myBitStream.Read(plrId);
			myBitStream.ReadVector(x, y, z);

			gPlayerInfo[plrId].id = plrId;
			gPlayerInfo[plrId].x = x;
			gPlayerInfo[plrId].y = y;
			gPlayerInfo[plrId].z = z;

			// printf("received plrId: %d %f %f %f\n", plrId, x, y, z);

			break;
		}
		default:
			break;
		}
	}
}

void SendPlayerPositions(RakNet::RakPeerInterface* server)
{
	for (int i = 0; i < 2; i++) {
		RakNet::BitStream bsOut;
		bsOut.Write((RakNet::MessageID)ID_PLAYER_COORDINATES);
		bsOut.Write((int32_t)i);
		bsOut.WriteVector(
			(float)gPlayerInfo[i].x,
			(float)gPlayerInfo[i].y,
			(float)gPlayerInfo[i].z
		);
		server->Send(&bsOut, LOW_PRIORITY, UNRELIABLE, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
	}
}

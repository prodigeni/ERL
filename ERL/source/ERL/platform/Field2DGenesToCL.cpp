#include <erl/platform/Field2DGenesToCL.h>
#include <erl/platform/RuleToCL.h>

using namespace erl;

// Buffer is organized like so:
// nodeOutput[0..n] + typeInput + nodeRecurrentData[0..n] + c * (connectionResponse[0..n] + typeInput + connectionRecurrentData[0..n])

std::string erl::field2DGenesNodeUpdateToCL(erl::Field2DGenes &genes, const std::vector<std::string> &functionNames, int fieldWidth, int fieldHeight, int connectionRadius) {
	int connectionDimensionSize = connectionRadius + 1;
	int numConnections = connectionDimensionSize * connectionDimensionSize;

	std::string code = "";

	// Add header
	code +=
		"/*\n"
		"ERL\n"
		"\n"
		"Generated OpenCL kernel\n"
		"*/\n"
		"\n"
		"// Dimensions of field\n"
		"constant int fieldWidth = " + std::to_string(fieldWidth) + ";\n"
		"constant int fieldHeight = " + std::to_string(fieldHeight) + ";\n"
		"constant float fieldWidthInv = " + std::to_string(1.0f / fieldWidth) + ";\n"
		"constant float fieldHeightInv = " + std::to_string(1.0f / fieldHeight) + ";\n"
		"\n"
		"// Connection offsets\n"
		"constant char2 offsets[" + std::to_string(numConnections) + "] = {\n";

	for (int x = -connectionRadius; x <= connectionRadius; x++) {
		code += "	";

		for (int y = -connectionRadius; y <= connectionRadius; y++) {
			code += "(char2)(" + std::to_string(x) + ", " + std::to_string(y) + "), ";
		}

		code += "\n";
	}

	code.pop_back();
	code.pop_back();
	code.pop_back();

	code +=
		"\n"
		"}\n"
		"\n"
		"// Connection update rule\n";

	// Generate rules for all nets
	neat::NetworkPhenotype connectionPT;

	connectionPT.create(genes.getConnectionUpdateGenotype());

	std::unordered_set<neat::NetworkPhenotype::Connection, neat::NetworkPhenotype::Connection> connectionData;
	std::vector<std::vector<size_t>> connectionOutgoingConnections;
	std::vector<bool> connectionRecurrentSourceNodes;
	size_t connectionNumRecurrentSourceNodes;

	code += ruleToCL(connectionPT, connectionData, connectionOutgoingConnections, connectionRecurrentSourceNodes, connectionNumRecurrentSourceNodes, "connectionRule", functionNames);

	code += "\n// Activation update rule\n";

	neat::NetworkPhenotype activationPT;

	activationPT.create(genes.getActivationUpdateGenotype());

	std::unordered_set<neat::NetworkPhenotype::Connection, neat::NetworkPhenotype::Connection> nodeData;
	std::vector<std::vector<size_t>> nodeOutgoingConnections;
	std::vector<bool> nodeRecurrentSourceNodes;
	size_t nodeNumRecurrentSourceNodes;

	code += ruleToCL(activationPT, nodeData, nodeOutgoingConnections, nodeRecurrentSourceNodes, nodeNumRecurrentSourceNodes, "activationRule", functionNames);

	int nodeSize = genes.getNodeOutputSize() + 2 + nodeNumRecurrentSourceNodes;
	int connectionSize = genes.getConnectionResponseSize() + 2 + connectionNumRecurrentSourceNodes;
	int nodeAndConnectionsSize = nodeSize + connectionSize * numConnections;

	// Other constants, and kernel definition
	code +=
		"\n"
		"// Data sizes\n"
		"constant int nodeAndConnectionsSize = " + std::to_string(nodeAndConnectionsSize) + ";\n"
		"constant int connectionSize = " + std::to_string(connectionSize) + ";\n"
		"constant int nodeSize = " + std::to_string(nodeSize) + ";\n"
		"\n"
		"// The kernel\n"
		"void kernel nodeUpdate(global const float* source, global float* destination, read_only image2d_t randomImage, float2 randomSeed) {\n"
		"	int nodeIndex = get_global_id(0);\n"
		"	int nodeStartOffset = nodeIndex * nodeAndConnectionsSize;\n"
		"	int connectionsStartOffset = nodeStartOffset + nodeSize;\n"
		"	int2 nodePosition = (int2)(nodeIndex % fieldWidth, nodeIndex / fieldHeight);\n"
		"	float2 normalizedCoords = (float2)nodePosition * float2(fieldWidthInv, fieldHeightInv);\n"
		"\n"
		"	// Update connections\n";

	// Add connection response accumulators
	for (int i = 0; i < genes.getConnectionResponseSize(); i++) {
		code += "	float responseSum" + std::to_string(i) + " = 0;\n";
	}

	code +=
		"\n"
		"	for (int ci = 0; ci < numConnections; ci++) {\n"
		"		int2 connectionNodePosition = nodePosition + offsets[ci]\n"
		"\n"
		"		// Wrap the coordinates around\n"
		"		connectionNodePosition.x = connectionNodePosition.x % fieldWidth;\n"
		"		connectionNodePosition.y = connectionNodePosition.x % fieldHeight;\n"
		"		connectionNodePosition.x = connectionNodePosition.x < 0 ? connectionNodePosition.x + fieldWidth : connectionNodePosition.x;\n"
		"		connectionNodePosition.y = connectionNodePosition.y < 0 ? connectionNodePosition.y + fieldHeight : connectionNodePosition.y;\n"
		"\n"
		"		int connectionNodeIndex = connectionNodePosition.x + connectionNodePosition.y * fieldWidth;\n"
		"		int connectionNodeStartOffset = connectionNodeIndex * nodeAndConnectionSize;\n"
		"		int connectionStartOffset = connectionsStartOffset + ci * connectionSize;\n"
		"\n";

	// Provide temporaries for holding outputs
	for (int i = 0; i < genes.getConnectionResponseSize(); i++) {
		code += "		float response" + std::to_string(i) + ";\n";
	}

	// Assign changeable recurrent values
	for (int i = 0; i < connectionNumRecurrentSourceNodes; i++) {
		code += "		float connectionRec" + std::to_string(i) + " =  source[connectionStartOffset + " + std::to_string(genes.getConnectionResponseSize() + 1 + i) + "];\n";
	}

	code += "\n"
		"		connectionRule(";

	// Add inputs
	for (int i = 0; i < genes.getNodeOutputSize(); i++) {
		code += "source[connectionStartOffset + " + std::to_string(i) + "], ";
	}

	// Type input
	code += "source[connectionStartOffset + " + std::to_string(genes.getNodeOutputSize()) + "], ";

	// Random input
	code += "read_imagef(randomImage, connectionNodePosition + nodePosition).x, ";

	// Add outputs
	for (int i = 0; i < genes.getConnectionResponseSize(); i++) {
		code += "&response" + std::to_string(i) + ", ";
	}

	// Add recurrent connections
	for (int i = 0; i < connectionNumRecurrentSourceNodes; i++) {
		code += "&connectionRec" + std::to_string(i) + ", ";
	}

	code.pop_back();
	code.pop_back();

	code +=
		");\n"
		"\n"
		"		// Add response to sum and assign to destination buffer\n";

	for (int i = 0; i < genes.getConnectionResponseSize(); i++) {
		code += "		destination[connectionNodeStartOffset + " + std::to_string(i) + "] = response" + std::to_string(i) + ";\n";
	}

	code +=
		"\n"
		"		// Accumulate response\n";

	for (int i = 0; i < genes.getConnectionResponseSize(); i++) {
		code += "		responseSum" + std::to_string(i) + " += response" + std::to_string(i) + ";\n";
	}

	code +=
		"\n"
		"		// Assign recurrent values to destination buffer\n";

	for (int i = 0; i < connectionNumRecurrentSourceNodes; i++) {
		code += "		destination[connectionStartOffset + " + std::to_string(genes.getNodeOutputSize() + 1 + i) + "] = connectionRec" + std::to_string(i) + ";\n";
	}

	// ----------------------------------------------------------- Finish block -----------------------------------------------------------

	code +=
		"	}\n"
		"\n";

	// Update activation
	for (int i = 0; i < genes.getNodeOutputSize(); i++) {
		code += "	float output" + std::to_string(i) + ";\n";
	}

	// Assign changeable recurrent values
	for (int i = 0; i < nodeNumRecurrentSourceNodes; i++) {
		code += "	float nodeRec" + std::to_string(i) + " =  source[nodeStartOffset + " + std::to_string(genes.getNodeOutputSize() + 1 + i) + "];\n";
	}

	code += "\n"
		"	activationRule(";

	// Add inputs
	for (int i = 0; i < genes.getConnectionResponseSize(); i++) {
		code += "responseSum" + std::to_string(i) + ", ";
	}

	// Type input
	code += "source[nodeStartOffset + " + std::to_string(genes.getConnectionResponseSize()) + "], ";

	// Random input
	code += "read_imagef(randomImage, nodePosition + int2(-1, -1)).x, ";

	// Add outputs
	for (int i = 0; i < genes.getNodeOutputSize(); i++) {
		code += "&output" + std::to_string(i) + ", ";
	}

	// Add recurrent connections
	for (int i = 0; i < nodeNumRecurrentSourceNodes; i++) {
		code += "&nodeRec" + std::to_string(i) + ", ";
	}

	code.pop_back();
	code.pop_back();

	code +=
		");\n"
		"\n"
		"	// Assign to destination buffer\n";

	for (int i = 0; i < genes.getConnectionResponseSize(); i++) {
		code += "	destination[nodeStartOffset + " + std::to_string(i) + "] = output" + std::to_string(i) + ";\n";
	}

	code +=
		"\n"
		"	// Assign recurrent values to destination buffer\n";

	for (int i = 0; i < connectionNumRecurrentSourceNodes; i++) {
		code += "	destination[nodeStartOffset + " + std::to_string(genes.getConnectionResponseSize() + 1 + i) + "] = nodeRec" + std::to_string(i) + ";\n";
	}

	// Finish kernel
	code +=
		"}";

	return code;
}
/*
ERL

Field2D Genes
*/

#pragma once

#include <neat/Evolvable.h>
#include <neat/NetworkGenotype.h>

namespace erl {
	class Field2DGenes : public neat::Evolvable {
	private:
		neat::NetworkGenotype _connectionUpdateGenotype;
		neat::NetworkGenotype _activationUpdateGenotype;
		neat::NetworkGenotype _typeSetGenotype;
		neat::NetworkGenotype _encoderGenotype;
		neat::NetworkGenotype _decoderGenotype;

		int _connectionResponseSize;
		int _nodeOutputSize;

	public:
		// Inherited from Evolvable
		void initialize(size_t numInputs, size_t numOutputs, const neat::EvolverSettings* pSettings, const std::vector<float> &functionChances, neat::InnovationNumberType &innovationNumber, std::mt19937 &generator);
		void crossover(const neat::EvolverSettings* pSettings, const std::vector<float> &functionChances, const Evolvable* pOtherParent, Evolvable* pChild, float fitnessForThis, float fitnessForOtherParent, neat::InnovationNumberType &innovationNumber, std::mt19937 &generator);
		void mutate(const neat::EvolverSettings* pSettings, const std::vector<float> &functionChances, neat::InnovationNumberType &innovationNumber, std::mt19937 &generator);
		float getSimilarity(const neat::EvolverSettings* pSettings, const std::vector<float> &functionChances, const Evolvable* pOther);

		const neat::NetworkGenotype &getConnectionUpdateGenotype() const {
			return _connectionUpdateGenotype;
		}

		const neat::NetworkGenotype &getActivationUpdateGenotype() const {
			return _activationUpdateGenotype;
		}

		const neat::NetworkGenotype &getTypeSetGenotype() const {
			return _typeSetGenotype;
		}

		const neat::NetworkGenotype &getEncoderGenotype() const {
			return _encoderGenotype;
		}

		const neat::NetworkGenotype &getDecoderGenotype() const {
			return _decoderGenotype;
		}

		int getConnectionResponseSize() const {
			return _connectionResponseSize;
		}

		int getNodeOutputSize() const {
			return _nodeOutputSize;
		}
	};
}
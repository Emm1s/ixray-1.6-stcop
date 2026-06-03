#include "stdafx.h"
#include "xrServer.h"

// Initiate migration
void xrServer::PerformMigration(CSE_Abstract* E, xrClientData* from, xrClientData* to)
{
	return;
//	R_ASSERT	(from != to);
//	NET_Packet	P;
//
//	// Send to current 'client' signal to deactivate 'entity'
//	{
//		P.w_begin			(M_MIGRATE_DEACTIVATE);
//		P.w_u16				(E->ID);
//		SendTo				(from->ID,P,net_flags(true,true));
//	}
//
//	// Send to _new_ 'client' signal to activate 'entity'
//	{
//		P.w_begin			(M_MIGRATE_ACTIVATE);
//		P.w_u16				(E->ID);
//		E->UPDATE_Write		(P);
//		SendTo				(to->ID,P,net_flags(true,true));
//	}
//
//	// Change parent-client
//	E->owner				= to;
}

#ifdef DEBUG
static size_t debug_num = 0;
static xr_set<ALife::_OBJECT_ID> UsedIDsDebug = {};

void xrServer::VerifyIDDebug()
{
	size_t result = 0;
	for (auto& elem1 : m_id_chunks)
	{
		for (auto& elem2 : elem1->data)
		{
			for (auto& elem3 : elem2.pack)
			{
				result += elem3.bset.n1;
				result += elem3.bset.n2;
				result += elem3.bset.n3;
				result += elem3.bset.n4;
				result += elem3.bset.n5;
				result += elem3.bset.n6;
				result += elem3.bset.n7;
				result += elem3.bset.n8;
			}
		}
	}
	VERIFY(result == debug_num);
}
#endif

void xrServer::clear_ids()
{
	xrCriticalSectionGuard g(m_id_chunksCS);
	m_id_chunks.clear();
#ifdef DEBUG
	debug_num = 0;
	UsedIDsDebug.clear();
#endif
}

ALife::_OBJECT_ID xrServer::PerformIDgen(ALife::_OBJECT_ID ID)
{
	xrCriticalSectionGuard g(m_id_chunksCS);
	// Clean-up too old pending ID
	if (m_id_chunks.empty()) // whatever the reason is, this means that all IDs (including pending) are invalidated
	{
		m_pending_delete_id_set.clear();
		while (!m_pending_delete_id_queue.empty()){
			m_pending_delete_id_queue.pop();
		}
	}
	else
	{
		IVERIFY(m_pending_delete_id_queue.size() >= m_pending_delete_id_set.size());
		while (!m_pending_delete_id_queue.empty())
		{
			auto& elem = m_pending_delete_id_queue.front();
			auto CurTime = Device.TimerAsync();
			if(elem.first + ID_delete_delay > CurTime)
			{
				//Msg("Stop ID [%u] free because timeout is not ready [current time: %u; queue time: %u; timeout: %u]", elem.second, CurTime, elem.first, ID_delete_delay);
				break;
			}
			//Msg("Free ID [%u] because timeout is ready [current time: %u; queue time: %u; timeout: %u]", elem.second, CurTime, elem.first, ID_delete_delay);
			if (m_pending_delete_id_set.contains(elem.second))
			{
				FreeIDImpl(elem.second);
				m_pending_delete_id_set.erase(elem.second);
			}
			m_pending_delete_id_queue.pop();
		}
	}
	
	// ID generation itself
	auto Result = ALife::INVALID_OBJECT_ID;
	size_t i1 = ID/i1Shift;
	size_t i2 = (ID%i1Shift)/i2Shift;
	size_t i3 = ((ID%i1Shift)%i2Shift)/i3Shift;
	const u8 Mod = ((ID%i1Shift)%i2Shift)%i3Shift;
	if (ID == ALife::INVALID_OBJECT_ID)
	{
		for (i1 = 0; i1 < m_id_chunks.size(); ++i1)
		{
			VERIFY(i1 < m_id_chunks.size());
			auto& Chunk = *m_id_chunks[i1];
			if (!Chunk.empty)
			{
				continue;
			}
			for (i2 = 0; i2 < 255; ++i2)
			{
				auto& LLChunk = Chunk.data[i2];
				if (!LLChunk.empty)
				{
					continue;
				}
				for (i3 = 0; i3 < 255; ++i3)
				{
					auto& Pack = LLChunk.pack[i3];
					if (Pack.set == 255)
					{
						continue;
					}
					auto CalcID = [&](u8 shift) -> ALife::_OBJECT_ID
					{
						return ALife::_OBJECT_ID(i1*i1Shift)
							+ ALife::_OBJECT_ID(i2*i2Shift)
							+ ALife::_OBJECT_ID(i3*i3Shift)
							+ shift;
					};
#ifdef DEBUG
#define PackAcquire(num) \
	if(!Pack.bset.n##num){ \
		Result = CalcID(num-1);\
		VERIFY(!UsedIDsDebug.contains(Result)); \
		Pack.bset.n##num = true; \
	}
#else
#define PackAcquire(num) \
	if(!Pack.bset.n##num){ \
	Result = CalcID(num-1);\
	Pack.bset.n##num = true; \
}
#endif
					PackAcquire(1)
					else PackAcquire(2)
					else PackAcquire(3)
					else PackAcquire(4)
					else PackAcquire(5)
					else PackAcquire(6)
					else PackAcquire(7)
					else PackAcquire(8)
#undef PackAcquire
					if (IVERIFY(Result != ALife::INVALID_OBJECT_ID))
					{
						if (Pack.set == 255)
						{
							IVERIFY(LLChunk.empty);
							--LLChunk.empty;
							if (!LLChunk.empty)
							{
								IVERIFY(Chunk.empty);
								--Chunk.empty;
							}
						}
						break;
					}
				}
				if (IVERIFY(Result != ALife::INVALID_OBJECT_ID))
				{
					break;
				}
			}
			if (IVERIFY(Result != ALife::INVALID_OBJECT_ID))
			{
				break;
			}
		}
		if (Result == ALife::INVALID_OBJECT_ID)
		{
			m_id_chunks.push_back(xr_make_unique<IDChunkSet>());
			auto& Chunk = *m_id_chunks.back();
			auto& LLChunk = Chunk.data[0];
			auto& Pack = LLChunk.pack[0];
			Pack.bset.n1 = true;
			Result = i1Shift*(m_id_chunks.size()-1);
			VERIFY(!UsedIDsDebug.contains(Result));
		}
		R_ASSERT(Result != ALife::INVALID_OBJECT_ID);
		if (Result > m_TopValidID || m_TopValidID == ALife::INVALID_OBJECT_ID)
		{
			m_TopValidID = Result;
		}
#ifdef DEBUG
		debug_num++;
		VerifyIDDebug();
		UsedIDsDebug.insert(Result);
#endif
		return Result;
	}
	{
		if (m_pending_delete_id_set.contains(ID))
		{
			m_pending_delete_id_set.erase(ID); // if we here, we need this ID right now, suppose it's safe to use
			return ID; // we haven't changed storage state for this ID, we can skip update and just return id
		}
		while (i1 >= m_id_chunks.size())
		{
			m_id_chunks.push_back(xr_make_unique<IDChunkSet>());
		}
		auto& Chunk = *(m_id_chunks[i1]);
		auto& LLChunk = Chunk.data[i2];
		auto& Pack = LLChunk.pack[i3];
		if (I_ASSERT_M(!(Pack.set & (1 << Mod)), "ID [%d] is already used!", ID))
		{
			Pack.set |= 1 << Mod;
			Result = ID;
		}
		if (Pack.set == 255)
		{
			IVERIFY(LLChunk.empty);
			--LLChunk.empty;
		}
		if (!LLChunk.empty)
		{
			IVERIFY(Chunk.empty);
			--Chunk.empty;
		}
	}
#ifdef DEBUG
	debug_num++;
	VerifyIDDebug();
	UsedIDsDebug.insert(Result);
#endif
	IVERIFY(ID == Result);
	if (Result > m_TopValidID || m_TopValidID == ALife::INVALID_OBJECT_ID)
	{
		m_TopValidID = Result;
	}
	return Result;
}

void xrServer::FreeID(ALife::_OBJECT_ID ID, u32 time)
{
	xrCriticalSectionGuard g(m_id_chunksCS);
	//Msg("Put ID [%u] in delay free [current time: %u]", ID, time);
	m_pending_delete_id_set.insert(ID);
	m_pending_delete_id_queue.emplace(time, ID);
}

ALife::_OBJECT_ID xrServer::TopValidID() const
{
	R_ASSERT(m_TopValidID != ALife::INVALID_OBJECT_ID);
	return m_TopValidID + 1;
}

void xrServer::FreeIDImpl(ALife::_OBJECT_ID ID)
{
	size_t i1 = ID/i1Shift;
	size_t i2 = (ID%i1Shift)/i2Shift;
	size_t i3 = ((ID%i1Shift)%i2Shift)/i3Shift;
	u8 Mod = ((ID%i1Shift)%i2Shift)%i3Shift;
	if (IVERIFY(i1 < m_id_chunks.size()))
	{
		auto& Chunk = *m_id_chunks[i1];
		auto& LLChunk = Chunk.data[i2];
		auto& Pack = LLChunk.pack[i3];
		bool PackBecameFree = Pack.set == 255;
		bool LLChunkBecameFree = !LLChunk.empty;
		if (IVERIFY((Pack.set & (1 << Mod))))
		{
			Pack.set &= ~(1 << Mod);
		}
		if (PackBecameFree)
		{
			++LLChunk.empty;
		}
		if (LLChunkBecameFree)
		{
			++Chunk.empty;
		}
	}
	
#ifdef DEBUG
	debug_num--;
	VerifyIDDebug();
	UsedIDsDebug.erase(ID);
#endif
}

#ifdef DEBUG
bool xrServer::IsIDUsed(ALife::_OBJECT_ID ID)
{
	xrCriticalSectionGuard g(m_id_chunksCS);
	size_t i1 = ID/i1Shift;
	size_t i2 = (ID%i1Shift)/i2Shift;
	size_t i3 = ((ID%i1Shift)%i2Shift)/i3Shift;
	u8 Mod = ((ID%i1Shift)%i2Shift)%i3Shift;
	if (IVERIFY(i1 < m_id_chunks.size()))
	{
		auto& Chunk = *m_id_chunks[i1];
		IVERIFY(Chunk.empty != 255);
		auto& LLChunk = Chunk.data[i2];
		IVERIFY(LLChunk.empty != 255);
		auto& Pack = LLChunk.pack[i3];
		if (IVERIFY((Pack.set & (1 << Mod))))
		{
			return Pack.set & (1 << Mod);
		}
	}
	return false;
}
#endif

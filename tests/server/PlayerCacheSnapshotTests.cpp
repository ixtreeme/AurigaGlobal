#include <Core/player_cache_snapshot.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace snap=auriga::player_cache_snapshot;
namespace ps=auriga::player_session;
namespace wire=auriga::player_cache_wire;
static unsigned checks;
static void Check(bool ok,const char* msg){++checks;if(!ok)throw std::runtime_error(msg);}
static ps::Token Token(uint8_t n){ps::Token t{};t.fill(n);return t;}
int main(){try{
 const ps::Key key{Token(1),Token(2),Token(3),42,900,100};
 snap::Snapshot state;state.account=900;state.player.id=100;state.player.exp=123456;
 const auto batch=snap::Batch(key,Token(4),77,state);Check(bool(batch),"native player snapshot refused");size_t at=0;
 auto u32=[&]{Check(at+4<=batch->size(),"truncated frame");uint32_t n=0;for(int shift=0;shift<32;shift+=8)n|=uint32_t((*batch)[at++])<<shift;return n;};
 Check((*batch)[at++]==HEADER_GD_PLAYER_SAVE,"player save must lead");
 Check(u32()==77 && u32()==sizeof(TPlayerTable),"player save scope or size");
 Check(!std::memcmp(batch->data()+at,&state.player,sizeof(state.player)),"player snapshot changed");at+=sizeof(state.player);
 Check((*batch)[at++]==HEADER_GD_PLAYER_CACHE_REQUEST,"retired item save emitted");
 Check(u32()==77 && u32()==wire::RequestBytes,"drain request scope or size");
 const auto request=wire::DecodeRequest({batch->data()+at,batch->size()-at});
 Check(request && request->key==key && request->challenge==Token(4) && request->action==wire::Action::Begin,"drain lost authentication");
 Check(at+wire::RequestBytes==batch->size(),"extra item frame appended");
 for(unsigned invalid=0;invalid<9;++invalid){auto k=key;auto s=state;auto challenge=Token(4);uint32_t descriptor=77;
  switch(invalid){case 0:k.link={};break;case 1:k.request={};break;case 2:k.session={};break;case 3:k.generation=0;break;case 4:k.character=0;break;case 5:++s.account;break;case 6:++s.player.id;break;case 7:challenge={};break;case 8:descriptor=0;break;}
  Check(!snap::Batch(k,challenge,descriptor,s),"invalid native snapshot admitted");}
 std::cout<<"native player snapshot: "<<checks<<" checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

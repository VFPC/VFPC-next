from reportlab.graphics.shapes import Drawing, Rect, String, Line, Polygon
from reportlab.lib import colors

def validation_diagram():
 d=Drawing(475,438)
 teal=colors.HexColor('#176B78');slate=colors.HexColor('#233746');pale=colors.HexColor('#EDF4F6')
 def box(x,y,w,h,label,fill=pale,text=slate):
  d.add(Rect(x,y,w,h,rx=5,ry=5,fillColor=fill,strokeColor=teal,strokeWidth=1))
  for j,line in enumerate(label.split('\n')):
   d.add(String(x+w/2,y+h/2+(len(label.split('\n'))-1)*6-j*12-3,line,fontName='Guide-Bold',fontSize=10.5,textAnchor='middle',fillColor=text))
 def arrow(x1,y1,x2,y2,label=None):
  d.add(Line(x1,y1,x2,y2,strokeColor=teal,strokeWidth=1.3))
  if y2<y1:d.add(Polygon([x2,y2,x2-3,y2+6,x2+3,y2+6],fillColor=teal,strokeColor=teal))
  elif x2<x1:d.add(Polygon([x2,y2,x2+6,y2+3,x2+6,y2-3],fillColor=teal,strokeColor=teal))
  if label:d.add(String((x1+x2)/2+9,(y1+y2)/2+2,label,fontName='Guide',fontSize=9.5,fillColor=slate))
 x=159;w=300;cx=x+w/2
 box(x,394,w,32,'IFR flight plan within controller scope')
 box(x,336,w,40,'Airport data and assigned\ndeparture runway available?')
 arrow(cx,394,cx,376)
 box(0,278,132,36,'PDG\nWait for required data',colors.HexColor('#FFF6DE'))
 arrow(x,356,66,356,'No');arrow(66,356,66,314)
 labels=[(278,'Syntax and departure checks'),(226,'Destination and exit-point validity'),(174,'Applicable restrictions and route'),(122,'Flight levels and odd/even rule'),(70,'Bans and warnings'),(18,'Validation result')]
 for y,label in labels:box(x,y,w,32,label,teal if y==18 else pale,colors.white if y==18 else slate)
 arrow(cx,336,cx,310,'Yes')
 for upper,lower in [(278,226),(226,174),(174,122),(122,70),(70,18)]:arrow(cx,upper,cx,lower+32)
 d.add(String(0,247,'Individual checks may depend',fontName='Guide',fontSize=9,fillColor=slate))
 d.add(String(0,234,'on earlier results.',fontName='Guide',fontSize=9,fillColor=slate))
 d.add(String(0,205,'A hard failure retains priority.',fontName='Guide',fontSize=9,fillColor=slate))
 d.add(String(0,192,'Warnings remain available',fontName='Guide',fontSize=9,fillColor=slate))
 d.add(String(0,179,'in Show Checks (FULL).',fontName='Guide',fontSize=9,fillColor=slate))
 return d

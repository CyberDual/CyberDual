const buffer = new ArrayBuffer(0, {maxByteLength: 4096});
buffer.transferToFixedLength();
%CollectGarbage(0);
